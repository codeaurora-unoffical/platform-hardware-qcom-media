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

#include "omx/preview_component.h"
#include "GroupsockHelper.hh"
#include <queue>
#include <condition_variable>
#include <algorithm>
#include <stdint.h>
#include <string.h>

namespace omxa {

class PreviewSource;

class RtpComponent : public std::enable_shared_from_this<RtpComponent>,
    public PreviewComponent, public OmxSink {

    OMX_BUFFERHEADERTYPE* buffer_in_use_ = NULL;   /* buffer currently being streamed from */
    std::queue<OMX_BUFFERHEADERTYPE*>  pending_;   /* queue of buffers pending to be streamed */
    unsigned pending_data_size_ = 0;
    std::condition_variable cv_;

    PreviewParameters params_;

    /* output_ member implements the FramedSource facet */
    std::weak_ptr<PreviewSource> output_;

    /* close */
    void close_locked(void) {
        source_ = NULL;
    }

    /** signal the output_ object to harvest the data */
    void signal_output(void);

public:

    RtpComponent(PreviewParameters& params) : params_(params) {}

    int init(PreviewParameters& params) { params_ = params; return 0; }

    virtual void close() {
        std::unique_lock<std::mutex> lk(lock_);
        close_locked();
    }

    virtual ~RtpComponent() { close(); }

    bool isOpen(void) {
        return true;
    }

    virtual bool isReadable() { return 0 < pending_data_size_;  }

    /** @note from buffer should be atleast 4 bytes long */
    inline uint32_t get4Bytes(uint8_t* from) {
        return (from[0]<<24)|(from[1]<<16)|(from[2]<<8)|from[3];
    }

    inline bool is_nal_prefix(uint32_t a_word, uint32_t& prefix_siz) {
        if (0x01 == a_word) {
            prefix_siz = 4;
            return true;
        }
        else if (0x01 == (a_word >> 8)) {
            prefix_siz = 3;
            return true;
        }

        return false;
    }

    inline void append_4(uint8_t*& to, unsigned int& to_size, uint32_t a_word) {
        if (4 <= to_size) {
            *to++ = (uint8_t)(a_word >> 24);
            *to++ = (uint8_t)(a_word >> 16);
            *to++ = (uint8_t)(a_word >>  8);
            *to++ = (uint8_t)a_word;
            to_size -= 4;
        }
        else {
            for (int i = 3; to_size && i; to_size--) {
                *to++ = (uint8_t)a_word >> (8 << i--);
            }
        }
    }

    /** copies a h264 nal unit looking for the nal prefix sequence. returns
       the number of bytes in the 'from' buffer scanned */
    uint32_t copyOneNAL(uint8_t* to, unsigned int to_size,
                        uint8_t* from, unsigned int from_size,
                        unsigned int& frame_size) {
        uint32_t prefix_siz;
        frame_size = 0;
        uint32_t src_bytes = from_size;

        /* skip until nal prefix */
        while (4 <= from_size
               && !is_nal_prefix(get4Bytes(from), prefix_siz)) {
            from++;
            from_size--;
        }

        if (4 <= from_size) {
            uint32_t next_word;

            from += prefix_siz;
            from_size -= prefix_siz;

            while (from_size) {
                if (4 <= from_size) {   /* scan 4 octets at a time for nal prefix */
                    next_word = get4Bytes(from);
                    if (is_nal_prefix(next_word, prefix_siz)) {
                        break;   /* end of frame */
                    }
                    else if (0x00 == (next_word&0xFF)) {
                        /* lsb is 0, potential prefix sequence following,
                           copy just one octet */
                        if (to_size) { *to++ = (uint8_t)(next_word >> 24); to_size--; }
                        frame_size++; from++; from_size--;
                    }
                    else {
                        /* copy all 4 octets from 'next_word' */
                        append_4(to, to_size, next_word);
                        frame_size += 4; from += 4; from_size -= 4;
                    }
                }
                else {  /* as there can't be a nal prefix at the end of from buffer */
                    if (to_size) { *to++ = *from; to_size--; }
                    frame_size++; from++; from_size--;
                }
            }

            return src_bytes - from_size;  /* number of bytes scanned */
        }
        return src_bytes;  /* skip over all of this data */
    }

    /**
     * get the data that corresponds with one presentation time. i.e the contents
     * of one omx buffer. This may include several NAL units including their
     * start code. This is more suitable for the live555 object
     * H264VideoStreamFramer.
     *
     * @param to : destination buffer to copy into.
     * @param to_size : size of destination buffer.
     * @param copied : number of octets copied.
     *
     * @return int : 0 on success or EINTR when the object is interrupted.
     * @note This is a blocking call if invoked when there are no more octets in
     *           the source. @sa isReadable()
     **/
    virtual int getData(uint8_t* to, uint32_t to_size, uint32_t& copied, uint32_t& truncated) {
        std::unique_lock<std::mutex> lk(lock_);

        if (NULL == buffer_in_use_) {
            cv_.wait(lk, [this](){return !pending_.empty();});
            buffer_in_use_ = pending_.front();
            pending_.pop();
        }

        /** @Note nFilledLen is end offset
         *  @Note nOffset is begin offset */
        copied = std::min<uint32_t>(
            to_size,
            (uint32_t)(buffer_in_use_->nFilledLen - buffer_in_use_->nOffset));
        truncated = 0;
        memmove(to, buffer_in_use_->pBuffer + buffer_in_use_->nOffset, copied);
        pending_data_size_ -= copied;
        buffer_in_use_->nOffset += copied;
        if (buffer_in_use_->nOffset == buffer_in_use_->nFilledLen) {  /* done with this buffer */
            /* return this buffer */
            (void) OMX_FillThisBuffer(source_, buffer_in_use_);
            buffer_in_use_ = NULL;
        }

        return 0; /* TODO: support EINTR */
    }

    /**
     * This will get one NAL unit at a time, but not including the start code at
     * the beginning. Compatible with the expectation of the live555
     * object H264VideoStreamDiscreteFramer as live source. This adheres to
     * recommendation by the author here :
     * http://lists.live555.com/pipermail/live-devel/2011-June/013412.html
     *
     * @param to : destination buffer to copy into.
     * @param to_size : size of the destination buffer.
     * @param copied : number of octets copied.
     * @param truncated : number of octets truncated in this NAL unit.
     *             i.e destination buffer is insufficient.
     *
     * @return int : 0 on success or EINTR when the object is interrupted.
     * @note This is a blocking call if invoked when there are no more octets in
     *           the source. @sa isReadable()
     **/
    virtual int getOneNalUnit(unsigned char* to, unsigned int to_size,
                              unsigned int& copied, unsigned int& truncated) {
        std::unique_lock<std::mutex> lk(lock_);

        if (NULL == buffer_in_use_) {
            cv_.wait(lk, [this](){return !pending_.empty();});
            buffer_in_use_ = pending_.front();
            pending_.pop();
        }

        /** @Note nFilledLen is end offset
         *  @Note nOffset is begin offset */
        uint32_t scanned = copyOneNAL(
            to, to_size, buffer_in_use_->pBuffer + buffer_in_use_->nOffset,
            (buffer_in_use_->nFilledLen - buffer_in_use_->nOffset), copied);
        pending_data_size_ -= scanned;
        buffer_in_use_->nOffset += scanned;

        /* copied is actually the frame_size, which may be larger than the
           'to' buffer provided, revise this for actual copied and account
           for truncated */
        if (to_size < copied) {
            truncated = copied - to_size;
            copied = to_size;
        }
        else {
            truncated = 0;
        }

        /* Are we done with this buffer? */
        if (buffer_in_use_->nOffset == buffer_in_use_->nFilledLen) {
            /* return the buffer to source component */
            (void) OMX_FillThisBuffer(source_, buffer_in_use_);
            buffer_in_use_ = NULL;
        }
        return 0; /* TODO: support EINTR */
    }

    /**
     Request to empty the contents of given omx buffer. The source of this data 
     is from an omx component initialized with openOmxSink() request. Once the 
     data is drained the buffer is returned back to the omx component via 
     OMX_FillThisBuffer(). 
     
     @param buf : OMX buffer
     
     @return OMX_ERRORTYPE 
     **/
    virtual OMX_ERRORTYPE emptyBuffer(OMX_BUFFERHEADERTYPE* buf) {
        std::unique_lock<std::mutex> lk(lock_);
        OMX_ERRORTYPE omxErr = OMX_ErrorNone;

        if (0 < buf->nFilledLen) {
            pending_.push(buf);
            pending_data_size_ += buf->nFilledLen;
            buf->nFilledLen += buf->nOffset;   /** @note We took control of this buffer,
                                                  from here on we treat its members
                                                  differently as below:
                                                      nFilledLen is end offset
                                                      nOffset as beginning offset */
            lk.unlock();
            cv_.notify_one();
            signal_output();
        }
        else {   /* return this buffer; todo: check for end-of-stream */
            omxErr = OMX_FillThisBuffer(source_, buf);
        }

        return omxErr;
    }

    virtual int openOMXSink(OMX_HANDLETYPE hComponent, OmxSinkPtr* ppout) {
        *ppout = shared_from_this();
        source_ = hComponent;
        return 0;
    }

    virtual int openDiscreteH264Source(UsageEnvironment& env,
        std::shared_ptr<FramedSource>& source_out);

    virtual int openH264Source(UsageEnvironment& env,
        std::shared_ptr<FramedSource>& source_out);
};

/**
 Implements a FramedSource contract. In this the data source will pass all the 
 H264 data. Notice the distinction with the \ref PreviewDiscreteSource. 
 **/
class PreviewSource : public FramedSource {
protected:
    std::shared_ptr<RtpComponent> me_;

    EventTriggerId signal_;
    TaskScheduler* task_;

    virtual void deliverFrame() {
        if (!isCurrentlyAwaitingData()) {
            // we're not ready for the data yet
            return;
        }

        me_->getData(fTo, fMaxSize, fFrameSize, fNumTruncatedBytes);
        gettimeofday(&fPresentationTime, NULL);

        // inform the reader that the data is written
        FramedSource::afterGetting(this);
    }

    static void deliverFrame0(void* clientData) {
        ((PreviewSource*)clientData)->deliverFrame();
    }

public:
    virtual ~PreviewSource() {}

    virtual void doGetNextFrame() {

        if (!me_->isOpen()) {
            handleClosure();
            return;
        }

        // If a new frame of data is immediately available to be delivered,
        // then retrieve now:
        if (me_->isReadable()) {
            deliverFrame();
        }
    }

    PreviewSource(UsageEnvironment& env, std::shared_ptr<RtpComponent> component)
        : FramedSource(env), me_(component) {
        task_ = &env.taskScheduler();
        signal_ = task_->createEventTrigger(deliverFrame0);
    }

    static void signal(PreviewSource& me) {

        if (NULL != me.task_) {
            me.task_->triggerEvent(me.signal_, &me);
        }
    }
};

/** implements deliverFrame() as a single NAL unit fetch */
class PreviewDiscreteSource : public PreviewSource {
private:

    virtual void deliverFrame() {
        if (!isCurrentlyAwaitingData()) {
            // we're not ready for the data yet
            return;
        }

        me_->getOneNalUnit(fTo, fMaxSize, fFrameSize, fNumTruncatedBytes);
        gettimeofday(&fPresentationTime, NULL);    /*TODO : record the presentation time when encoder
                                                     gives the buffers. */

        // inform the reader that the data is written
        PreviewSource::afterGetting(this);
    }

public:
    PreviewDiscreteSource(UsageEnvironment& env, std::shared_ptr<RtpComponent> component)
        : PreviewSource(env, component) {}
};

/**
 * This will return a FramedSource instances for use with
 * H264VideoStreamDiscreteFramer as a live source. In this object the
 * doGetNextFrame() will fetch one NAL unit at a time without the start code.
 *
 * @param env : live555 Environment
 * @param source_out : FramedSource instance. Follow the shared_ptr ownership
 *           policy with this object.
 *
 * @return int : 0 on success
 **/
int RtpComponent::openDiscreteH264Source(UsageEnvironment& env,
    std::shared_ptr<FramedSource>& source_out) {
    std::shared_ptr<PreviewSource> pout = output_.lock();
    if (!pout) {
        pout = std::make_shared<PreviewDiscreteSource>(env, shared_from_this());
        output_ = pout;
    }

    source_out = pout;

    return 0;
}

/**
 This will return a FramedSource instances for use with
 H264VideoStreamFramer as a live source.

 @param env : live555 Environment
 @param source_out : FramedSource instance. Follow the shared_ptr ownership
           policy with this object.

 @return int : 0 on success
**/
int RtpComponent::openH264Source(UsageEnvironment& env,
    std::shared_ptr<FramedSource>& source_out) {
    std::shared_ptr<PreviewSource> pout = output_.lock();
    if (!pout) {
        pout = std::make_shared<PreviewSource>(env, shared_from_this());
        output_ = pout;
    }

    source_out = pout;

    return 0;
}

/** notify PreviewSource to doGetNextFrame() */
void RtpComponent::signal_output(void)
{
    std::shared_ptr<PreviewSource> pout = output_.lock();
    if (pout) {
        PreviewSource::signal(*pout);
    }
}

int PreviewComponent::create(PreviewParameters& params, PreviewComponentPtr* out)
{
    std::shared_ptr<RtpComponent> pout = std::make_shared<RtpComponent>(params);

    if (!pout) {
        return ENOMEM;
    }

    int rc = pout->init(params);

    if (0 == rc) {
        *out = pout;
    }

    return rc;
}

}

