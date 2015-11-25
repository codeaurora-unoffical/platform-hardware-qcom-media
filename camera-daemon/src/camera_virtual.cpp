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
#include <string.h>
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <algorithm>

/** TODO: Need MT-safety */

class CameraVirtualParams : public camera::ICameraParameters {
    std::map<std::string, std::string> keyval_;
public:

    CameraVirtualParams(camera::ICameraDevice* icd = NULL, const char* s = NULL) {
        if (NULL != icd) {
            (void)init(icd);
        }

        if (NULL != s) {
            update(s);
        }
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
};

class CameraVirtual
: public camera::ICameraDevice, public camera::ICameraListener {
    camera::ICameraDevice* icd_;
    CameraVirtualParams params_;

    std::vector<camera::ICameraListener*> listeners_;

    bool isPreviewStarted_ = false;
    bool isRecordingStarted_ = false;

    std::string previewResolution_ = "1280x720";

    inline int startPlaying() {
        return icd_->setParameters(params_) || icd_->startPreview();
    }

    /** Camera listener methods */
    virtual void onError() {
        /** TODO: */
    }
    virtual void onVideoFrame(camera::ICameraFrame* frame) {
        /** TODO: */
        QCAM_ERR("Video frame unexpected on this virtual camera");
    }
    virtual void onPreviewFrame(camera::ICameraFrame* frame) {
        /** TODO:
         *  enqueue for processing in a separate thread */
        if (isRecordingStarted_) {
            for (auto& l : listeners_) {
                l->onVideoFrame(frame);
            }
        }
        else if (isPreviewStarted_) {
            for (auto& l : listeners_) {
                l->onPreviewFrame(frame);
            }
        }
    }
public:

    CameraVirtual(camera::ICameraDevice* icd) : icd_(icd), params_(icd) {
        icd_->addListener(this);
        // dumps the device params to stdout
        // params_.writeObject(std::cout);
    }

    virtual ~CameraVirtual() {
        camera::ICameraDevice::deleteInstance(&icd_);
    }

    int init() {
        int rc = 0;

        params_.update("preview-size=1280x720;"
                       "preferred-preview-size-for-video=1280x720;"
                       "preview-frame-rate=30;"
                       "preview-frame-rate-values=30;"
                       "preview-fps-range=30000,30000;"
                       "preview-fps-range-values=(30000,30000);"
                       "preview-format=nv12-venus;"
                       "preview-format-values=nv12-venus;"
                       "video-hfr=off;"
                       "video-hfr-values=off;"
                       "video-hsr=off;"
                       "video-size=3840x2160;");
        return rc;
    }

    virtual void addListener(camera::ICameraListener *listener) {
        for (auto& l : listeners_) {
            if (l == listener) {
                return;
            }
        }

        listeners_.push_back(listener);
    }

    virtual void removeListener(camera::ICameraListener *listener) {

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

        int rc = params.writeObject(os);

        if (0 == rc) {
            params_.update(buffer.str());
        }

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

        /** ensure the preview size is programmed */
        TRY(rc, params_.getValue("preview-size", previewResolution_));  /** save a copy */
        params_.setValue("preview-size", previewResolution_);

        TRY(rc, startPlaying());
        isPreviewStarted_ = true;

        CATCH(rc) {}
        return rc;
    }

    virtual void stopPreview() {
        isPreviewStarted_ = false;
        if (!isRecordingStarted_) {
            icd_->stopPreview();
        }
    }

    virtual int startRecording() {
        std::string resolution;
        int rc = 0;

        if (isRecordingStarted_) {
            THROW(rc, EALREADY);
        }

        if (isPreviewStarted_) {
            icd_->stopPreview();
        }

        /** use the programmed video size on preview stream */
        TRY(rc, params_.getValue("preview-size", previewResolution_));  /** save a copy */
        TRY(rc, params_.getValue("video-size", resolution));
        params_.setValue("preview-size", resolution);

        /** and start the preview */
        TRY(rc, startPlaying());

        isRecordingStarted_ = true;

        CATCH(rc) {}
        return rc;
    }

    virtual void stopRecording() {
        isRecordingStarted_ = false;
        icd_->stopPreview();

        if (isPreviewStarted_) {  /** start with preview resolution */
            params_.setValue("preview-size", previewResolution_);
            startPlaying();
        }
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

int CameraVirtual_CreateInstance(int idx, camera::ICameraDevice** po)
{
    return CameraVirtual::create(idx, po);
}
