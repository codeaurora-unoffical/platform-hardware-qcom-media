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

#ifndef __OMXA_BUFFER_POOL_H__
#define __OMXA_BUFFER_POOL_H__

#include "OMX_Core.h"
#include "OMX_Component.h"
#include <stdint.h>
#include <memory>
#include <cassert>
#include <mutex>
#include <list>
#include <condition_variable>

namespace omxa {

class BufferPool;
typedef std::shared_ptr<BufferPool> BufferPoolPtr;

/**
 Represents a BufferPool object, constructs individual buffers out of the
 given port on the omx component.
 
 Offers MT-safe allocation of a buffer out of the pool
 **/
class BufferPool {
    OMX_HANDLETYPE h_;
    OMX_U32 port_;
    OMX_S32 bufferCount_ = 0;
    OMX_S32 bufferSize_ = 0;
    OMX_BUFFERHEADERTYPE** buffers_ = NULL;  /** an array of OMX_BUFFERHEADERTYPE* */

    /** a container of free buffers */
    std::list<OMX_BUFFERHEADERTYPE*> free_;
    std::mutex lock_;
    std::condition_variable cv_;
    bool ready_ = false;   /* used to interrupt threads waiting on this object */

    OMX_ERRORTYPE init(OMX_HANDLETYPE h, OMX_U32 port, int bufferCount,
                       int bufferSize);
    void final(void);

protected:
    BufferPool() {}
    BufferPool(const BufferPool&) = delete;
    const BufferPool& operator =(const BufferPool&) = delete;

    static BufferPoolPtr make_shared() {
        struct make_shared_enabler : public BufferPool {};
        return std::make_shared<make_shared_enabler>();
    }
public:

    ~BufferPool() { final(); }

    /**
     * get the reference to a buffer in the pool at a given index.
     * @param idx
     * @return OMX_BUFFERHEADERTYPE*&
     **/
    OMX_BUFFERHEADERTYPE*& at(const int idx) {
        assert(idx >= 0 && idx < bufferCount_);
        return buffers_[idx];
    }

    /**
     * supporting methods for running an iterator on all the buffers in this
     * pool
     */
    typedef OMX_BUFFERHEADERTYPE** iterator;
    typedef OMX_BUFFERHEADERTYPE** const_iterator;
    iterator begin() { return &buffers_[0]; }
    iterator end() { return &buffers_[bufferCount_]; }

    /**
     releases the buffer. This may cause pre-emption.
     @param buf
     **/
    void releaseBuf(OMX_BUFFERHEADERTYPE* buf);

    /**
     allocate a buffer. This may block until a buffer becomes available.
     @return OMX_BUFFERHEADERTYPE*
     **/
    OMX_BUFFERHEADERTYPE* allocBuf(void);

    /**
     * Create a buffer pool from the given component's port.
     *
     * @param h : an omx component
     * @param port : input/output number port of the component
     * @param bufferCount : number of buffers to create
     * @param bufferSize : size of each buffer
     * @param pa :[out] the object on success
     *
     * @return OMX_ERRORTYPE : OMX_ErrorNone on success.
     **/
    static OMX_ERRORTYPE create(OMX_HANDLETYPE h, OMX_U32 port,
        int bufferCount, int bufferSize, BufferPoolPtr* pa) {
        std::shared_ptr<BufferPool> a = make_shared();

        *pa = a;

        return (NULL != a) ? a->init(h, port, bufferCount, bufferSize)
            : OMX_ErrorInsufficientResources;
    }
};

} /* namespace omxa */
#endif /* !__OMXA_BUFFER_POOL_H__ */

