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
#ifndef __OMXA_VIDEO_ENCODER_COMPONENT_H__
#define __OMXA_VIDEO_ENCODER_COMPONENT_H__

#include "buffer_pool.h"
#include "OMX_Core.h"
#include "OMX_Component.h"

namespace omxa {

/**
 * An adapter to the OMX IL.
 **/
class OmxSource {
    OMX_HANDLETYPE source_ = NULL;
    BufferPoolPtr pool_;
    friend class EncoderComponent;

    int open(OMX_HANDLETYPE hComponent, BufferPoolPtr& pool) {

        close();
        pool_ = pool;
        source_ = hComponent;
        return 0;
    }
    void close() {
        source_ = NULL;
        pool_.reset();
    }
protected:
    OmxSource(){}
    OmxSource(const OmxSource&) = delete;
    const OmxSource& operator =(const OmxSource&) = delete;
public:
    ~OmxSource() { close(); }
    inline bool isOpen() {
        return (NULL != source_);
    }

    /**
     * Invoke this method to release the buffers dispatched in to the drainer.
     * Usually from the OMX_CALLBACKTYPE::EmptyBufferDone().
     * @param hComponent
     * @param pBuffer
     * @return OMX_ERRORTYPE
     **/
    OMX_ERRORTYPE fillThisBuffer(OMX_HANDLETYPE hComponent,
                                 OMX_BUFFERHEADERTYPE* pBuffer) {
        pBuffer->nFlags = 0;
        return OMX_FillThisBuffer(hComponent, pBuffer);
    }
};

typedef std::shared_ptr<OmxSource> OmxSourcePtr;


/**
 * Video encoder, simplified presentation to OMX IL.
 **/
class EncoderComponent {
    OMX_HANDLETYPE h_;     /**< a omx component being wrapped by this class */
    OMX_STATETYPE current_;  /**< current state of the omx component */
    OMX_STATETYPE requested_;  /**< requested state to transition */

    std::mutex lock_;
    std::condition_variable cv_;

    OmxSource output_;   /**< one output port */

protected:
    EncoderComponent(const EncoderComponent&) = delete;
    const EncoderComponent& operator =(const EncoderComponent&) = delete;

public:
    EncoderComponent() : h_(NULL), current_(OMX_StateInvalid),
        requested_(OMX_StateInvalid) {}

    void reset() {
        output_.close();
        h_ = NULL;
        current_ = OMX_StateInvalid;
        requested_ = OMX_StateInvalid;
    }

    int init(OMX_HANDLETYPE& h, OMX_STATETYPE init_state = OMX_StateMax) {
        int nret = 0;

        if (NULL != h_) {
            reset();
        }

        h_ = h;

        if (OMX_StateMax == init_state) {
            OMX_ERRORTYPE omxError;
            omxError = OMX_GetState(h, &init_state);
            if (OMX_ErrorNone != omxError) {
                nret = ENXIO;
                goto bail;
            }
        }
        current_ = init_state;
        requested_ = init_state;
    bail:
        return nret;
    }

    void set(OMX_STATETYPE s) {
        std::unique_lock<std::mutex> lk(lock_);

        current_ = s;

        lk.unlock();

        cv_.notify_all();
    }

    /**
     Initiate the transition of associated omx component. A successful return 
     from this function implies the request has been accepted, but the component 
     may not go in to the state immediately. Use wait_until() to ensure the state 
     is reached. 
     
     @param s : state to enter.
     
     @return OMX_ERRORTYPE 
     **/
    OMX_ERRORTYPE enter(OMX_STATETYPE s) {
        std::unique_lock<std::mutex> lk(lock_);
        OMX_ERRORTYPE omxError = OMX_ErrorNone;

        if (requested_ != s) {

            requested_ = s;
            omxError = OMX_SendCommand(h_, OMX_CommandStateSet, (OMX_U32)s, NULL);
        }

        return omxError;
    }

    /**
     Wait until the associated omx component reaches the provided state.
     
     @param s : wait (block) until this state is reached.
     **/
    void wait_until(OMX_STATETYPE s) {
        std::unique_lock<std::mutex> lk(lock_);

        cv_.wait(lk, [this, &s] { return (current_ == s) ? true : false; });
    }

    /**
     Initiate the transition of omx component to the give state and wait until
     it acknowledges the completion.
     
     @param [in] s : state to transition.
     
     @return OMX_ERRORTYPE 
     **/
    OMX_ERRORTYPE enter_and_wait_until(OMX_STATETYPE s) {
        OMX_ERRORTYPE omxError = OMX_ErrorNone;
        std::unique_lock<std::mutex> lk(lock_);

        if (requested_ != s) {

            requested_ = s;
            /* yikes:( unlock and reacquire around the OMX_SendCommand()
               because its implementation is a blocking call, which opens up a
               deadlock with in its threads.
               TODO: remove sem_wait() in OMX_SendCommand() */
            lk.unlock();
            omxError = OMX_SendCommand(h_, OMX_CommandStateSet, (OMX_U32)s, NULL);
            lk.lock();

            if (OMX_ErrorNone == omxError) {
                cv_.wait(lk, [this, &s] { return (current_ == s) ? true : false; });
            }
        }
        return omxError;
    }

    int openOMXSource(BufferPoolPtr& buffers, OmxSourcePtr* ppout) {
        int nret = 0;
        if (output_.isOpen()) {
            return EALREADY;
        }

        nret = output_.open(h_, buffers);
        if (0 == nret) {
            *ppout = OmxSourcePtr(&output_, [](OmxSource* p){ p->close(); });
        }

        return nret;
    }
};

} /* namespace omxa */

#endif /* !__OMXA_VIDEO_ENCODER_COMPONENT_H__ */

