/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "camerad_util.h"
#include "qcamvid_log.h"
#include "camera.h"
#include "frame_scaler.h"

#include <string.h>
#include <cassert>
#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <algorithm>
#include <atomic>
#include <thread>
#include <list>
#include <queue>
#include <condition_variable>

#include <sys/stat.h>  /* O_RDONLY */
#include <fcntl.h>

#include <unistd.h>  /* close() */
#include <sys/ioctl.h>  /* ioctl() */
#include <sys/mman.h>  /* mmap() and munmap() */
#include <linux/msm_ion.h>
#include <cutils/native_handle.h>

#include <media/msm_media_info.h>

#define ALIGN(x, a)     (((x) + (a) - 1) & ~((a) - 1))

using namespace camerad;

class CameraVirtualFramePool;
typedef std::shared_ptr<CameraVirtualFramePool> CameraVirtualFramePoolPtr;

class CameraVirtualFrame : public camera::ICameraFrame {
    #define NH_NUM_FDS 1    /**< used for tunneling data buffer */
    #define NH_NUM_INTS 2   /**< used for tunneling data buffer */

    std::atomic_ulong ref_cnt_ = {0};

    std::weak_ptr<CameraVirtualFramePool> container_weak_;
    CameraVirtualFramePoolPtr container_ = nullptr;

    /**< a handle to ion buffer */
    ion_user_handle_t ion_handle_ = NULL;

    /**< a special formatting for tunnelling a shared buffer.
     * +-----------------------------------+
     * |  buffer type  | native_handle_t*  |
     * |   (4 bytes)   |   (4 bytes)       |
     * +-----------------------------------+
     **/
    struct {
        uint32_t type_;
        native_handle_t* nh_ptr_;
    } tunnel_;

    /**< pre-allocated memory for native_handle_t with data */
    uint8_t nh_[sizeof(native_handle_t) + (NH_NUM_FDS + NH_NUM_INTS) * sizeof(int)];

    void initTunnel(void) {
        tunnel_.nh_ptr_ = (native_handle_t*)nh_;

        /* initialize the native handle */
        tunnel_.nh_ptr_->numFds = NH_NUM_FDS;
        tunnel_.nh_ptr_->numInts = NH_NUM_INTS;
        tunnel_.nh_ptr_->data[0] = fd;
        tunnel_.nh_ptr_->data[1] = 0; /**< offset */
        tunnel_.nh_ptr_->data[2] = size;

        /* 0 is a default source type */
        tunnel_.type_ = 0;

        /* initialize the ICameraFrame member */
        metadata = &tunnel_;
    }

public:

    int init(CameraVirtualFramePoolPtr container, int ion_fd, size_t sz,
             size_t align) {
        struct ion_allocation_data d1 = {0};
        struct ion_fd_data d2 = {0};
        int rc = 0;

        d1.len = sz;
        d1.align = align;
        d1.flags = ION_FLAG_CACHED;
        d1.heap_mask = ION_HEAP(ION_IOMMU_HEAP_ID);
        TRY(rc, ioctl(ion_fd, ION_IOC_ALLOC, &d1));

        d2.handle = d1.handle;
        TRY(rc, ioctl(ion_fd, ION_IOC_SHARE, &d2));

        container_weak_ = container;   /* the container reference */

        /* move ion handle in to frame.*/
        ion_handle_ = d1.handle; d1.handle = NULL;
        fd   = d2.fd;
        size = sz;

        /* map the memory in to local process */
        data = (uint8_t*)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (NULL == data) {
            THROW(rc, EFAULT);
        }

        initTunnel(); /** complete initializing the framedata tunneling structures */

        CATCH(rc) {
            if (NULL != d1.handle) {
                ioctl(fd, ION_IOC_FREE, &d1);
            }
        }
        return rc;
    }

    void final(int ion_fd) {
        assert(0 == ref_cnt_);

        /** unmap virtual address */
        if (NULL != data) { munmap(data, size); data = NULL; }

        /** close the file descriptor */
        if (-1 < fd) { close(fd); fd = -1; }

        /** free the ion buffer handle */
        if (NULL == ion_handle_) {
            struct ion_handle_data handle_data = {0};
            handle_data.handle = ion_handle_;
            ion_handle_ = NULL;
            ioctl(ion_fd, ION_IOC_FREE, &handle_data);
        }

        container_weak_.reset();
    }

    virtual uint32_t acquireRef() {
        if (nullptr == container_) {
            container_ = container_weak_.lock();
        }
        return ++ref_cnt_;
    }

    virtual uint32_t releaseRef();
};

class CameraVirtualFramePool
: public std::enable_shared_from_this<CameraVirtualFramePool> {
    /** a container of free frames */
    std::list<CameraVirtualFrame*> free_;
    std::mutex lock_;
    std::condition_variable cv_;
    bool ready_ = false;   /* used to interrupt threads waiting on this object */
    CameraVirtualFrame* frames_ = NULL;  /**< an array of frame buffers */
    uint32_t count_ = 0;  /**< count of ion buffers */
    uint32_t size_ = 0;   /**< size of ion buffer */

protected:
    CameraVirtualFramePool() {}
    CameraVirtualFramePool(const CameraVirtualFramePool&) = delete;
    const CameraVirtualFramePool& operator =(const CameraVirtualFramePool&) = delete;

    static CameraVirtualFramePoolPtr make_shared() {
        struct make_shared_enabler : public CameraVirtualFramePool {};
        return std::make_shared<make_shared_enabler>();
    }

    int init(uint32_t count, size_t size) {
        int rc = 0;
        int ion_fd = -1;
        size_t page_size = (size_t)sysconf(_SC_PAGESIZE);

        frames_ = new CameraVirtualFrame[count];
        if (NULL == frames_) {
            THROW(rc, ENOMEM);
        }

        count_ = count;

        size_ = ALIGN(size, page_size);    /**< make sure the size is page aligned */

        ion_fd = open("/dev/ion", O_RDONLY);
        if (ion_fd < 0) {
            THROW(rc, EBUSY);
        }

        for (uint32_t i = 0; i < count; i++) {
            TRY(rc, frames_[i].init(shared_from_this(), ion_fd, size_, page_size));
            release(&frames_[i]); /* add to the pool for future allocation */
        }

        ready_ = true;  /** mark the frame pool is ready for service */

        CATCH(rc) {}

        if (-1 < ion_fd) { close(ion_fd); ion_fd = -1; }
        return rc;
    }

    void final(void) {

        if (NULL != frames_) {
            std::unique_lock<std::mutex> lk(lock_);
            int rc = 0;
            int ion_fd = -1;

            ready_ = false;   /* interrupt any waiting threads */
            lk.unlock();
            cv_.notify_all();

            ion_fd = open("/dev/ion", O_RDONLY);
            if (ion_fd < 0) {
                THROW(rc, EBUSY);   /** leak the frames?? */
            }

            /** TODO:  test all allocated frames are released. */

            for (uint32_t i = 0; i < count_; i++) {
                frames_[i].final(ion_fd);
            }

            CATCH(rc) {}

            delete [] frames_; frames_ = NULL;
            count_ = 0;
            size_ = 0;
            if (-1 < ion_fd) { close(ion_fd); ion_fd = -1; }
        }
    }

public:
    ~CameraVirtualFramePool() {
        final();
    }

    void release(CameraVirtualFrame* f) {
        std::unique_lock<std::mutex> lk(lock_);

        free_.push_back(f);

        lk.unlock();
        cv_.notify_one();
    }

    CameraVirtualFrame* alloc(void) {
        CameraVirtualFrame* f = NULL;
        std::unique_lock<std::mutex> lk(lock_);

        /** wait until the free_ list is non-empty; interrupt if no longer ready */
        cv_.wait(lk, [this](){return !free_.empty() || !ready_;});

        if (!ready_) {  /* i.e interrupted */
            return NULL;
        }
        f = free_.front();
        free_.pop_front();

        f->acquireRef();
        return f;
    }

    static int create(int count, int size, CameraVirtualFramePoolPtr* pa) {
        CameraVirtualFramePoolPtr a = make_shared();

        *pa = a;

        return (NULL != a) ? a->init(count, size) : ENOMEM;
    }
};

uint32_t CameraVirtualFrame::releaseRef()
{

    if (ref_cnt_ <= 0) {
        return ref_cnt_;
    }

    --ref_cnt_;

    unsigned long refs = ref_cnt_;  /* save on the stack so that the container_.reset()
                                    ** may actually destroy this object. Which may result in
                                    ** recursively access to ref_cnt_ member after delete */
    if (0 == refs) {
        /** release the frame to pool */
        if (nullptr != container_) {
            container_->release(this);
            container_.reset();
        }
    }

    return refs;
}

class CameraVirtualParams : public camera::ICameraParameters {
    std::map<std::string, std::string> keyval_;
public:
    CameraVirtualParams() {}

    CameraVirtualParams(camera::ICameraDevice* icd) {
        (void)init(icd);
    }

    int init(camera::ICameraDevice* icd) {
        int rc = 0;
        int paramBufSize = 0;
        uint8_t* paramBuf = 0;

        rc = icd->getParameters(0, 0, &paramBufSize);
        if (0 != rc) {
            goto bail;
        }

        paramBuf = (uint8_t*)calloc(paramBufSize+1, 1);
        if (0 == paramBuf) {
            rc = ENOMEM;
            goto bail;
        }

        rc = icd->getParameters(paramBuf, paramBufSize);
        if (0 != rc) {
            goto bail;
        }

        /* clear the map and re-populate with the device params */
        keyval_.clear();
        update((const char*)paramBuf);

    bail:
       free(paramBuf);
       return rc;
    }

    virtual int writeObject(std::ostream& ps) const {
        for (std::map<std::string, std::string>::const_iterator i = keyval_.begin();
              i != keyval_.end(); i++) {
            ps << i->first << "=" << i->second << ";";
        }
        return 0;
    }

    void update(const std::string& params) {
        const char *a = params.c_str();
        const char *b;

        for (;;) {
            // Find the bounds of the key name.
            b = strchr(a, '=');
            if (b == 0) {
                break;
            }

            // Create the key string.
            std::string k(a, (size_t)(b-a));

            // Find the value.
            a = b+1;
            b = strchr(a, ';');
            if (b == 0) { // If there's no semicolon, this is the last item.
                std::string v(a);
                keyval_[k] = v;
                break;
            }

            std::string v(a, (size_t)(b-a));
            keyval_[k] = v;
            a = b+1;
        }
    }

    int getValue(const std::string& key, std::string& value) const {
        std::map<std::string, std::string>::const_iterator i = keyval_.find(key);
        if (i == keyval_.end()) {
            return ENOENT;
        }
        value = i->second;
        return 0;
    }

    void setValue(const std::string& key, const std::string& val) {
        keyval_[key] = val;
    }

    static int validate(const std::string& str) {
        CameraVirtualParams params;
        int rc;
        std::string val;

        params.update(str);

        TRY(rc, params.getValue("preview-frame-rate", val));
        if (val != "30") {
            THROW(rc, EINVAL);
        }

        TRY(rc, params.getValue("preview-fps-range", val));
        if (val != "30000,30000") {
            THROW(rc, EINVAL);
        }

        CATCH(rc) {}
        return rc;
    }
};

class ICameraVirtualListener {
public:
    virtual ~ICameraVirtualListener() {}
    virtual void onVirtualFrame(camera::ICameraFrame* fLow,
                                camera::ICameraFrame* fHigh) = 0;
};

class CameraVirtualPreview {
    std::atomic_bool stop_ = {false};   /**< when true stops the preview dispatch thread in doPreview() */
    std::atomic_bool pause_ = {false};  /**< when true the input frames are ignored until this state is cleared */

    std::thread worker_;  /**< a worker thread */
    std::condition_variable cv_; /**< conditional variable while waiting */
    std::mutex lock_;   /**< serialize the access to this object */
    std::queue<camera::ICameraFrame*>  pending_;   /**< queue of buffers pending
                                  for processing by c2d scaling */
    CameraVirtualFramePoolPtr scaled_down_frames_;
    const size_t POOL_SIZE_SCALED_DOWN_FRAMES = 7;  /**< enough buffers for down stream encoding */

    ICameraVirtualListener& listener_;

    ICameraFrameScalerPtr scaler_;

    int inWidth_, inHeight_, outWidth_, outHeight_;   /**< given to scaler for frame processing */

    void doPreview() {
        std::unique_lock<std::mutex> lk(lock_);
        camera::ICameraFrame* srcFrame = NULL;
        CameraVirtualFrame* dstFrame = NULL;
        while (!stop_) {
            /** wait until the pending_ list is non-empty; interrupt if no
             *  longer opened */
            cv_.wait(lk, [this](){return !pending_.empty() || stop_;});

            if (stop_) {
                break;
            }

            /* pick up the frame */
            srcFrame = pending_.front();
            pending_.pop();

            lk.unlock();

            dstFrame = scaled_down_frames_->alloc();

            if (NULL != dstFrame) {
                /** scaling */
                scaler_->convert(srcFrame, dstFrame);

                dstFrame->timeStamp = srcFrame->timeStamp;
                /** invoke listener */
                listener_.onVirtualFrame(static_cast<camera::ICameraFrame*>(dstFrame),
                                         static_cast<camera::ICameraFrame*>(srcFrame));
                dstFrame->releaseRef(); dstFrame = NULL;
            }

            /* release the src frame */
            srcFrame->releaseRef(); srcFrame = NULL;

            lk.lock();
        }
    }

protected:
    CameraVirtualPreview(ICameraVirtualListener& listener) : listener_(listener)
    {}

public:

    virtual ~CameraVirtualPreview(){ stop(); };

    int start(int inWidth, int inHeight, int outWidth, int outHeight) {
        int rc = 0;
        std::unique_lock<std::mutex> lk(lock_);

        if (worker_.joinable()) {
            QCAM_INFO("CameraVirtualPreview : EALREADY");
            return EALREADY;
        }

        /** TODO: error checking */
        rc = ICameraFrameScaler::create(inWidth, inHeight,
                                        outWidth, outHeight,
                                        &scaler_);

        rc = CameraVirtualFramePool::create(
            POOL_SIZE_SCALED_DOWN_FRAMES,
            VENUS_BUFFER_SIZE(COLOR_FMT_NV12, outWidth, outHeight),
            &scaled_down_frames_);

        if (EXIT_SUCCESS == rc) {
            inWidth_ = inWidth;
            inHeight_ = inHeight;
            outWidth_ = outWidth;
            outHeight_ = outHeight;

            worker_ = std::thread(&CameraVirtualPreview::doPreview, this);

            /* unlock and synchronize with future */
            lk.unlock();

            QCAM_INFO("start CameraVirtualPreview; pool->use_count : %d",
                      scaled_down_frames_.use_count());
        }

        return rc;
    }

    void pause() {
        std::unique_lock<std::mutex> lk(lock_);
        pause_ = true;
        /* release all pending frames */
        while (!pending_.empty()) {
            camera::ICameraFrame* srcFrame = NULL;
            srcFrame = pending_.front();
            pending_.pop();
            srcFrame->releaseRef();
        }
    }

    void resume(int inWidth, int inHeight) {

        /** use the suitable scaler */
        /** TODO: error checking */
        (void) ICameraFrameScaler::create(inWidth, inHeight,
                                            outWidth_, outHeight_,
                                            &scaler_);
        inWidth_ = inWidth;
        inHeight_ = inHeight;

        std::unique_lock<std::mutex> lk(lock_);
        pause_ = false;
    }

    void stop() {
        std::unique_lock<std::mutex> lk(lock_);
        if (worker_.joinable()) {
            stop_ = true;
            lk.unlock();

            QCAM_INFO("stop CameraVirtualPreview");

            cv_.notify_all();
            worker_.join();
            lk.lock();
            /* release all pending frames */
            while (!pending_.empty()) {
                camera::ICameraFrame* srcFrame = NULL;
                srcFrame = pending_.front();
                pending_.pop();
                srcFrame->releaseRef();
            }
            /* release frame pool for scale operations */
            scaled_down_frames_.reset();
            /* release the scaler */
            scaler_.reset();
            stop_ = false;
            QCAM_INFO("stop CameraVirtualPreview; pool->use_count : %d",
                      scaled_down_frames_.use_count());
        }
    }

    void enqueue(camera::ICameraFrame* f) {
        std::unique_lock<std::mutex> lk(lock_);
        if (!pause_ && worker_.joinable()) {
            f->acquireRef();   /* hold on to a reference as this frame will be
                               ** asynchronously processed */
            pending_.push(f);
            lk.unlock();
            cv_.notify_one();
        }
    }

    static int create(ICameraVirtualListener& l, CameraVirtualPreview** pout) {
        *pout = new CameraVirtualPreview(l);
        return 0;
    }
};

/**
 implements a virtual camera device over a physical device. In this the virtual
 camera offers a limited use case to achieve 30fps concurrent video recording
 and previewing streams. As the physical device isn't able to meet the processing
 demand, this object manages the downscaling on the GPU. C2D is a linux library
 wrapper over the firm that runs on the GPU.
 **/
class CameraVirtual
: public camera::ICameraDevice, public camera::ICameraListener,
  public ICameraVirtualListener {

    camera::ICameraDevice* icd_;   /**< underlying physical device */
    CameraVirtualParams params_;   /**< caller facing params */
    CameraVirtualParams params_d_; /**< device facing params */

    /**< user facing listeners are added here */
    std::vector<camera::ICameraListener*> listeners_;

    bool isPreviewStarted_ = false;
    bool isRecordingStarted_ = false;

    /**< Used to save the previewing (lower) resolution when the underlying physical
       device is set to a higher resolution. This is the target resolution for
       scaling down on GPU */
    std::string previewResolution_ = "1280x720";
    std::string recordingResolution_ = "3840x2160";

    CameraVirtualPreview* preview_ = 0;

    /**< program the parameters and start the previewing */
    inline int startPlaying(bool bVideo) {
        uint32_t inWidth, inHeight, outWidth, outHeight;
        int rc = 0;

        /** establish frame resolution for preview thread processing */
        TRY(rc, getResolution(
            bVideo ? recordingResolution_.c_str() : previewResolution_.c_str(),
            inWidth, inHeight));
        TRY(rc, getResolution(previewResolution_.c_str(), outWidth, outHeight));

        /** start the preview thread */
        TRY(rc, preview_->start(inWidth, inHeight, outWidth, outHeight));

        /** use the programmed video size on preview stream */
        params_d_.setValue("preview-size",
                           bVideo ? recordingResolution_ : previewResolution_);
        TRY(rc, icd_->setParameters(params_d_));

        /** start the source from device */
        TRY(rc, icd_->startPreview());

        CATCH(rc) {}
        return rc;
    }

    inline void stopPlaying() {
        preview_->stop();
        icd_->stopPreview();
    }

    /** program the suitable parameters and resume the previewing */
    inline int resumePlaying(bool bVideo) {
        uint32_t inWidth, inHeight;
        int rc = 0;

        /** establish frame resolution for preview thread processing */
        TRY(rc, getResolution(
            bVideo ? recordingResolution_.c_str() : previewResolution_.c_str(),
            inWidth, inHeight));
        /** resume the preview thread processing */
        preview_->resume(inWidth, inHeight);

        /** use the programmed video size on preview stream */
        params_d_.setValue("preview-size",
                           bVideo ? recordingResolution_ : previewResolution_);
        TRY(rc, icd_->setParameters(params_d_));

        /** start the source from device */
        TRY(rc, icd_->startPreview());

        CATCH(rc) {}
        return rc;
    }

    /** pause the previewing before stopping device */
    inline void pausePlaying() {
        preview_->pause();
        icd_->stopPreview();
    }

    /** Camera listener methods */
    virtual void onError() {
        /** TODO: */
    }

    virtual void onVideoFrame(camera::ICameraFrame* frame) {
        QCAM_ERR("Video frame unexpected on this virtual camera");
    }

    virtual void onPreviewFrame(camera::ICameraFrame* frame) {
        /** enqueue for processing in a separate thread */
        preview_->enqueue(frame);
    }

    virtual void onPictureFrame(camera::ICameraFrame *frame) {
        /** TODO: iterator needs guard */
        for (auto& l : listeners_) {
            l->onPictureFrame(frame);
        }
    }

    virtual void onVirtualFrame(camera::ICameraFrame* fLow,
                                camera::ICameraFrame* fHigh){

        /** TODO: iterator needs guard */
        if (isRecordingStarted_) {
            for (auto& l : listeners_) {
                l->onVideoFrame(fHigh);
            }
        }

        if (isPreviewStarted_) {
            for (auto& l : listeners_) {
                l->onPreviewFrame(fLow);
            }
        }
    }

    /**
     extract the width and height from string of the format : "1280x720"
     @param s : null terminated string to extract from
     @param [out] width : width of the frame
     @param [out] height : height of the frame
     @return int 0 on success or one of the errors in errno.h
     **/
    inline int getResolution(const char* s, uint32_t& width, uint32_t& height) {
        char* p;
        width = strtoul(s, &p, 10); /* decimal */
        if (p && *p++ == 'x' && *p) {
            height = strtoul(p, &p, 10);
            return 0;
        }
        return ENODATA;
    }

public:

    CameraVirtual(camera::ICameraDevice* icd)
    : icd_(icd), params_(icd), params_d_(icd) {
        icd_->addListener(this);
        // dumps the device params to stdout
        // params_.writeObject(std::cout);
    }

    virtual ~CameraVirtual() {
        if (0 != preview_) {
            preview_->stop();
            delete preview_;
            preview_ = 0;
        }
        camera::ICameraDevice::deleteInstance(&icd_);
    }

    int init() {
        int rc = 0;
        const char* useParams =
            "preview-size=1280x720;"
            "preferred-preview-size-for-video=1280x720;"
            "preview-frame-rate=30;"
            "preview-fps-range=30000,30000;"
            "preview-format=nv12-venus;"
            "video-hfr=off;"
            "video-hsr=off;"
            "video-size=3840x2160;"
            "recording-hint=true;";   /* recording-hint needs to be set for preview to stay in fixed fps */
        const char* supportingParams =
            "preview-fps-range-values=(30000,30000);"
            "preview-frame-rate-values=30;"
            "preview-format-values=nv12-venus;"
            "video-hfr-values=off;";

        TRY(rc, CameraVirtualPreview::create(*this, &preview_));

        params_.update(useParams);
        params_d_.update(useParams);

        /** publish only the supported values on this virtual device
         *  so that it doesn't change the supported values in physical device */
        params_.update(supportingParams);

        CATCH(rc) {}
        return rc;
    }

    virtual void addListener(camera::ICameraListener *listener) {
        /** TODO: needs guard */
        for (auto& l : listeners_) {
            if (l == listener) {
                return;
            }
        }

        listeners_.push_back(listener);
    }

    virtual void removeListener(camera::ICameraListener *listener) {
        /** TODO: needs guard */

        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(),
                                     listener),
                         listeners_.end());
    }

    virtual void subscribe(uint32_t eventMask) {
        icd_->subscribe(eventMask);
    }

    virtual void unsubscribe(uint32_t eventMask) {
        icd_->unsubscribe(eventMask);
    }

    virtual int setParameters(const camera::ICameraParameters& params) {
        std::stringbuf buffer;
        std::ostream os(&buffer);  // associate stream buffer to stream
        int rc;

        TRY(rc, params.writeObject(os));

        TRY(rc, CameraVirtualParams::validate(buffer.str()));

        params_.update(buffer.str());

        CATCH(rc) {}
        return rc;
    }

    virtual int getParameters(uint8_t* buf, uint32_t bufSize,
                              int* bufSizeRequired) {
        std::stringbuf buffer;
        std::ostream os(&buffer);  // associate stream buffer to stream

        int rc = params_.writeObject(os);

        if (0 == rc) {
            uint32_t len = buffer.str().length();
            memmove(buf, buffer.str().c_str(), std::min(bufSize, len));
            if (0 != bufSizeRequired) {
                *bufSizeRequired = len;
            }
        }
        return rc;
    }

    virtual int startPreview() {
        int rc = 0;

        if (isPreviewStarted_) {
            THROW(rc, EALREADY);
        }

        if (isRecordingStarted_) {
            THROW(rc, EBUSY);
        }

        /** ensure the preview size is programmed */
        TRY(rc, params_.getValue("preview-size", previewResolution_));  /** save a copy */
        TRY(rc, startPlaying(false));
        isPreviewStarted_ = true;

        CATCH(rc) {}
        return rc;
    }

    virtual void stopPreview() {
        isPreviewStarted_ = false;
        if (!isRecordingStarted_) {
            stopPlaying();
        }
    }

    virtual int startRecording() {
        int rc = 0;

        if (isRecordingStarted_) {
            THROW(rc, EALREADY);
        }

        /** save a copy */
        TRY(rc, params_.getValue("preview-size", previewResolution_));
        TRY(rc, params_.getValue("video-size", recordingResolution_));

        if (isPreviewStarted_) {
            pausePlaying();
            TRY(rc, resumePlaying(true));
        }
        else {

            /** and start */
            TRY(rc, startPlaying(true));
        }

        isRecordingStarted_ = true;

        CATCH(rc) {}
        return rc;
    }

    virtual void stopRecording() {
        isRecordingStarted_ = false;

        if (isPreviewStarted_) {
            pausePlaying();
            (void)resumePlaying(false);
        }
        else {
            stopPlaying();
        }
    }

    virtual int takePicture() {
        return icd_->takePicture();
    }

    static int create(int idx, ICameraDevice** device) {
        int rc = 0;
        ICameraDevice* icd = 0;
        CameraVirtual* me = 0;

        TRY(rc, camera::ICameraDevice::createInstance(idx, &icd));

        me = new CameraVirtual(icd);
        if (0 == me) {
            THROW(rc, ENOMEM);
        }
        icd = 0;

        TRY(rc, me->init());

        CATCH(rc) {
            camera::ICameraDevice::deleteInstance(&icd);
            delete me; me = 0;
        }

        *device = me;

        return rc;
    }
};

extern "C" {
int CameraVirtual_CreateInstance(int idx, camera::ICameraDevice** po)
{
    QCAM_INFO("idx : %d\n", idx);

    return CameraVirtual::create(idx, po);
}
}
