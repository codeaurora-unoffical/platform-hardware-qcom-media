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
#ifndef __OMXA_CAMERA_COMPONENT_H__
#define __OMXA_CAMERA_COMPONENT_H__

#include "camera.h"
#include "camera_parameters.h"
#include "buffer_pool.h"
#include "OMX_Core.h"
#include <chrono>
#include <memory>
#include <stdint.h>
#include <mutex>

namespace omxa {

/** An adapter to the OMX IL. */
class OmxDrain {
    OMX_HANDLETYPE drainer_ = NULL;
    std::recursive_mutex lock_;
    BufferPoolPtr pool_;
    friend class CameraComponent;

    /**
     * Dispatch the frame in to OpenMAX IL. Assumes the component is ready to
     * handle the metadata (the file descriptor of the memory block in the
     * buffers) as described by the OpenMax extension
     * "OMX.google.android.index.storeMetaDataInBuffers".
     *
     * Reference to camera frame is held until the omx component is done with it.
     * omx component must acknowledge by invoking the OmxDrain::emptyBufferDone().
     * Failing to acknowledge will result in the memory leak and CameraComponent
     * will stall the data path.
     *
     * @param frame : a frame from camera
     * @param ts : time stamp associated with the frame
     *
     * @return int : 0 on success or else failed code.
     **/
    int dispatchOmx(camera::ICameraFrame* frame, OMX_TICKS ts);
    int open(OMX_HANDLETYPE hComponent, BufferPoolPtr& pool);
    void close_locked();
    void close();
protected:
    OmxDrain(){}
    OmxDrain(const OmxDrain&) = delete;
    const OmxDrain& operator =(const OmxDrain&) = delete;
public:
    ~OmxDrain() { close(); }
    inline bool isOpen() {
        return (NULL != drainer_);
    }

    /**
     * Invoke this method to release/return the buffers dispatched in to the
     * drainer. Usually from the callback OMX_CALLBACKTYPE::EmptyBufferDone().
     *
     * This invocation will indicate the CameraComponent this buffer is available
     * to fill in a new frame. Failing to invoke this method on a frame
     * dispatched to omx component will result in CameraComponent eventually
     * stalling on the data path.
     *
     * @param hComponent
     * @param pBuffer
     * @return OMX_ERRORTYPE
     **/
    OMX_ERRORTYPE emptyBufferDone(OMX_HANDLETYPE hComponent,
                                  OMX_BUFFERHEADERTYPE* pBuffer);
};

typedef std::shared_ptr<OmxDrain> OmxDrainPtr;

/**
 Assemble frame statistics.
**/
class FrameStats {
    uint32_t frameCount_ = 0;
    uint32_t overrunCount_ = 0;
    std::chrono::microseconds timePrevious_;
    std::chrono::microseconds timeDiff_;
    float curFps_ = 0.0f;

public:
    void onNewFrame(camera::ICameraFrame* frame,
                    const std::chrono::microseconds& ts);
    void onOverrun() {
        overrunCount_++;
    }
    void reset();
};

/**
 A bridge between camera object and OMX IL. Provides the framework to integrate
 with an OpenMax video encoder
 **/
class CameraComponent : private camera::ICameraListener {

protected:
    CameraComponent(const CameraComponent&) = delete;
    const CameraComponent& operator =(const CameraComponent&) = delete;

public:

    /** camera component may support concurrent streams at different
       resolutions. usually at the same frame rate (fps) */
    enum CameraStream {
        STREAM_PREVIEW = 0, /**< a preview stream is usually intended to be a low resolution.
                            the term preview comes from the usage in phone applications.  */
        STREAM_VIDEO   = 1, /**< a stream usually intended to be higher resolution. */
    };

    const static uint32_t numStreams_ = 2;

    CameraComponent() {}
    virtual ~CameraComponent();

    /**
     * close all streams and remove association with the camera object.
     **/
    void reset(void);

    /**
     * Associate a camera object with this component.
     * @param icd
     * @return int
     **/
    int init(camera::ICameraDevice* icd);

    /**
     * Open an omx port to a drainer component. A drainer component implements
     * the OMX IL for recieving the data. This implementation invokes the omx
     * APIs dispatching the data when CameraDevice starts the stream.
     *
     * @param stream : Camera stream to associate the drainer with.
     * @param hComponent : omx drainer component
     * @param pool : A buffer pool for passing data into the omx component
     * @param ppout : Handle to the OmxDrain object.
     *
     * @return int
     **/
    int openOMXDrain(enum CameraStream stream, OMX_HANDLETYPE hComponent,
                     BufferPoolPtr& pool, OmxDrainPtr* ppout);

    /**
     * Break the association with the drainer component. Note there may be a
     * frame in dispatch with the drainer component that may cause the run-time
     * exception. Caller must stop the streams before breaking the media graph.
     * @param s
     **/
    void closeOMXDrain(enum CameraStream s);

private:
    OmxDrain drain_[numStreams_];
    FrameStats stat_[numStreams_];  /**< stats for diagnostics use */

    camera::ICameraDevice* camera_ = NULL; /**< camera obj */

    void processFrame(CameraStream sid, camera::ICameraFrame* frame);

    /** Camera listener methods */
    virtual void onError();
    virtual void onVideoFrame(camera::ICameraFrame* frame) {
        processFrame(STREAM_VIDEO, frame);
    }
    virtual void onPreviewFrame(camera::ICameraFrame *frame) {
        processFrame(STREAM_PREVIEW, frame);
    }
};

} /* namespace omxa */

#endif /* !__OMXA_CAMERA_COMPONENT_H__ */
