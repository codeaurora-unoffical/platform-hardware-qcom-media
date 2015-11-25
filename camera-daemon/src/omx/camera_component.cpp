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
#include "camera_component.h"
#include "OMX_Component.h"
#include "qcamvid_log.h"

using namespace omxa;

/**
 * Dispatch the frame in to OpenMAX IL. Assumes the component is ready to handle
 * the metadata (the file descriptor of the memory block). Reference to camera
 * frame is held until the omx component is done with it. omx component must
 * acknowledge by invoking the OmxDrain::emptyBufferDone().
 *
 * @param frame
 * @param ts
 *
 * @return int : 0 on success or else failed code.
 **/
int OmxDrain::dispatchOmx(camera::ICameraFrame* frame, OMX_TICKS ts)
{
    OMX_BUFFERHEADERTYPE* buffer = 0;
    int nret = 0;
    std::unique_lock<std::recursive_mutex> lk(lock_);

    if (!isOpen()) {
        nret = EIO;
        goto bail;
    }

    buffer = pool_->allocBuf();
    if (NULL == buffer) {
        nret = ENOMEM;
        goto bail;
    }

    /* initialize the buffer */
    buffer->nFilledLen = frame->size;
    buffer->nTimeStamp = ts;
    buffer->nOffset = 0;
    buffer->nFlags = 0;
    buffer->pBuffer = (OMX_U8*)frame->metadata;
    buffer->pAppPrivate = frame;

    /** Keep the video frame around until drainer is done with it */
    frame->acquireRef();

    if (OMX_ErrorNone != OMX_EmptyThisBuffer(drainer_, buffer)) {
        nret = EIO;
        frame->releaseRef();
        pool_->releaseBuf(buffer);
        goto bail;
    }

bail:
    return nret;
}

/**
 * drainer response that it is done with the buffer, release the camera frame.
 *
 * @param hComponent
 * @param pBuffer
 *
 * @return OMX_ERRORTYPE
 **/
OMX_ERRORTYPE OmxDrain::emptyBufferDone(OMX_HANDLETYPE hComponent,
                                        OMX_BUFFERHEADERTYPE* pBuffer)
{
    std::unique_lock<std::recursive_mutex> lk(lock_);

    if (!isOpen()) {
        return OMX_ErrorInvalidState;
    }

    camera::ICameraFrame* frame = (camera::ICameraFrame*)pBuffer->pAppPrivate;

    if (NULL != frame) {
        frame->releaseRef();
    }

    pool_->releaseBuf(pBuffer);

    return OMX_ErrorNone;
}

int OmxDrain::open(OMX_HANDLETYPE hComponent, BufferPoolPtr& pool)
{
    std::unique_lock<std::recursive_mutex> lk(lock_);
    close_locked();

    pool_ = pool;
    drainer_ = hComponent;

    return 0;
}

void OmxDrain::close_locked()
{
    if (isOpen()) {
        drainer_ = NULL;
        pool_.reset();
    }
}

void OmxDrain::close()
{
    std::unique_lock<std::recursive_mutex> lk(lock_);
    close_locked();
}

void FrameStats::onNewFrame(camera::ICameraFrame* frame,
                            const std::chrono::microseconds& ts)
{
    /* the fps sampling for debug purposes */
    #define FPS_SAMPLING_FRAME_COUNT 30
    static const uint64_t USEC_PER_SEC = 1000000;

    frameCount_++;

    /* maintain a running average for time difference between two frames
       to keep track of the FPS */
    timeDiff_ = std::chrono::microseconds(
        (timeDiff_.count() + (ts.count() - timePrevious_.count())) / 2);
    timePrevious_ = ts;

    curFps_ = USEC_PER_SEC / timeDiff_.count();

    if (frameCount_ % FPS_SAMPLING_FRAME_COUNT == 0) {
        QCAM_INFO("video FPS = %.2f", curFps_);
    }
}

void FrameStats::reset()
{
    frameCount_ = 0;
    curFps_ = 0;
}

CameraComponent::~CameraComponent()
{
}

/**
 * Open the omx drainer. Establish the connection with the IL component that will
 * drain the frame using the buffer headers.
 *
 * @param hComponent
 * @param buffers
 * @param bufferCount
 *
 * @return int
 **/
int CameraComponent::openOMXDrain(
    enum CameraStream stream,
    OMX_HANDLETYPE hComponent,
    BufferPoolPtr& pool,
    OmxDrainPtr* ppout)
{
    int nret = 0;
    if (drain_[stream].isOpen()) {
        return EALREADY;
    }

    nret = drain_[stream].open(hComponent, pool);
    if (0 == nret) {
        *ppout = OmxDrainPtr(&drain_[stream], [](OmxDrain* p){ p->close(); });
    }

    return nret;
}

void CameraComponent::closeOMXDrain(enum CameraStream stream)
{
    return drain_[stream].close();
}

void CameraComponent::onError()
{
    QCAM_ERR("camera error!\n");
}

/**
 * Process the video frame.
 * Collect any stats and dispatch to active drainer.
 *
 * @param frame
 **/
void CameraComponent::processFrame(CameraStream sid, camera::ICameraFrame* frame)
{

    /* skip, when there is no IL drain associated. */
    if (!drain_[sid].isOpen()) {
        return;
    }

    std::chrono::microseconds timeCurrent;

    /* current frame timestamp in microseconds */
    timeCurrent = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::nanoseconds(frame->timeStamp));

    /* update stats */
    stat_[sid].onNewFrame(frame, timeCurrent);

    /* dispatch */
    if (0 != drain_[sid].dispatchOmx(
        frame, (OMX_TICKS)timeCurrent.count())) {

        stat_[sid].onOverrun();
    }
}

/**
 * Initialize the camera component with the camera instance.
 *
 * @param icd
 * @return int
 **/
int CameraComponent::init(camera::ICameraDevice* icd)
{
    int nret = 0;

    if (0 != camera_) {
        camera_->removeListener(this);
        camera_ = NULL;
    }

    if (0 != icd) {
        camera_ = icd;
        camera_->addListener(this);
    }

    return nret;
}

void CameraComponent::reset(void)
{
    for (uint32_t i = 0; i < numStreams_; i++) {
        drain_[i].close();
        stat_[i].reset();
    }
    if (0 != camera_) {
        camera_->removeListener(this);
        camera_ = NULL;
    }
}

