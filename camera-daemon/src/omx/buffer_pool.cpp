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
#include "buffer_pool.h"

using namespace omxa;

/*******************************************************************************
 * Initialize a bufferpool object by creating buffers out of an omx port.
 * @param h : Handle to OMX component
 * @param port : input or output port identifier on the component
 * @param bufferCount : number of buffers
 * @param bufferSize : size of each buffer
 *
 * @return OMX_ERRORTYPE
 ******************************************************************************/
OMX_ERRORTYPE BufferPool::init(OMX_HANDLETYPE h, OMX_U32 port,
                               int bufferCount, int bufferSize)
{
    OMX_ERRORTYPE omxError = OMX_ErrorNone;

    h_ = h;
    port_ = port;
    bufferCount_ = bufferCount;
    bufferSize_ = bufferSize;

    /** allocate the array  */
    buffers_ = (OMX_BUFFERHEADERTYPE**)calloc(
        bufferCount_, sizeof(OMX_BUFFERHEADERTYPE*));
    if (NULL == buffers_) {
        return OMX_ErrorInsufficientResources;
    }

    /** initialize the array with bufferheaders */
    for (int i = 0; i < bufferCount_; i++) {
        omxError = OMX_AllocateBuffer(h_, &buffers_[i],
            port_, this, bufferSize_);
        if (OMX_ErrorNone != omxError) {
            goto bail;
        }

        buffers_[i]->pAppPrivate = NULL;
        free_.push_back(buffers_[i]); /* mark as available */
    }

    ready_ = true;  /** mark the buffer pool is ready for service */

bail:
    if (OMX_ErrorNone != omxError) { final(); }
    return omxError;
}

/*******************************************************************************
 * Finalize the bufferpool object, interrupt if any threads are waiting for free
 * buffers.
 ******************************************************************************/
void BufferPool::final(void)
{

    if (NULL != buffers_) {
        std::unique_lock<std::mutex> lk(lock_);

        ready_ = false;   /* interrupt any waiting threads */
        lk.unlock();
        cv_.notify_all();

        for (int i = 0; i < bufferCount_; i++) {
            if (NULL != buffers_[i]) {
                OMX_FreeBuffer(h_, port_, buffers_[i]);
                buffers_[i] = NULL;
            }
        }
        bufferCount_ = 0;

        free(buffers_);
        buffers_ = NULL;
    }
}

/*******************************************************************************
 * releases the buffer. This may cause pre-emption.
 * @param buf
 ******************************************************************************/
void BufferPool::releaseBuf(OMX_BUFFERHEADERTYPE* buf)
{
    std::unique_lock<std::mutex> lk(lock_);

    buf->pAppPrivate = NULL;

    free_.push_back(buf);
    lk.unlock();
    cv_.notify_one();
}

/*******************************************************************************
 * allocate a buffer. This may block until a buffer becomes available.
 * @return OMX_BUFFERHEADERTYPE*
 ******************************************************************************/
OMX_BUFFERHEADERTYPE* BufferPool::allocBuf(void)
{
    OMX_BUFFERHEADERTYPE* buffer = NULL;
    std::unique_lock<std::mutex> lk(lock_);

    /** wait until the free_ list is non-empty; interrupt if no longer ready */
    cv_.wait(lk, [this](){return !free_.empty() || !ready_;});

    if (!ready_) {  /* i.e interrupted */
        return NULL;
    }
    buffer = free_.front();
    free_.pop_front();

    return buffer;
}

