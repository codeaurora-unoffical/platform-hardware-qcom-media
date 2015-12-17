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
#include "qcamvid_session.h"
#include "camera_factory.h"
#include "omx/buffer_pool.h"
#include "omx/encoder_configure.h"
#include "camerad.h"
#include "camerad_util.h"
#include "qcamvid_log.h"
#include "camera.h"
#include "camera_parameters.h"
#include "omx/camera_component.h"
#include "omx/file_component.h"
#include "omx/encoder_component.h"
#include "omx/preview_component.h"
#include <media/hardware/HardwareAPI.h>
#include <memory>
#include <atomic>

/* OMX video encoder port identifiers */
enum PortIndexType {
    PORT_INDEX_IN = 0,
    PORT_INDEX_OUT = 1,
    PORT_INDEX_BOTH = -1,
    PORT_INDEX_NONE = -2
};

static const int ENCODER_OUTPUT_BUF_COUNT = 10;
static const int ENCODER_INPUT_BUF_COUNT = 7;
static const int ENCODER_DEFAULT_BITRATE = 5000000; // 5 Mbps
static const int ENCODER_DEFAULT_FRAMERATE = 30;

static const int CAMERA_RESOLUTION_DEFAULT_WIDTH = 1280;
static const int CAMERA_RESOLUTION_DEFAULT_HEIGHT = 720;

namespace camerad
{

class VSession : public ISession {
protected:
    /* General VSession Vars */
    SessionConfig mConfig;
    omx::video::encoder::EncoderConfigType encoderConfig_ = {
        .eCodec = OMX_VIDEO_CodingAVC,
        .eCodecProfile = omx::video::encoder::AVCProfileHigh,
        .eCodecLevel = omx::video::encoder::DefaultLevel,
        .eControlRate = OMX_Video_ControlRateVariable,
        .nResyncMarkerSpacing = 0,
        .eResyncMarkerType = omx::video::encoder::RESYNC_MARKER_NONE,
        .nIntraRefreshMBCount = 0,
        .nFrameWidth = CAMERA_RESOLUTION_DEFAULT_WIDTH,
        .nFrameHeight = CAMERA_RESOLUTION_DEFAULT_HEIGHT,
        .nOutputFrameWidth = CAMERA_RESOLUTION_DEFAULT_WIDTH,
        .nOutputFrameHeight = CAMERA_RESOLUTION_DEFAULT_HEIGHT,
        .nDVSXOffset = 0,
        .nDVSYOffset = 0,
        .nBitrate = ENCODER_DEFAULT_BITRATE,
        .nFramerate = ENCODER_DEFAULT_FRAMERATE,
        .nRotation = 0,
        .nInBufferCount = ENCODER_INPUT_BUF_COUNT,
        .nOutBufferCount = ENCODER_OUTPUT_BUF_COUNT,
        .cInFileName = "",
        .cOutFileName = "",
        .nFrames = 30,
        .nIntraPeriod = ENCODER_DEFAULT_FRAMERATE * 2,
        .nMinQp = 2,
        .nMaxQp = 31,
        .bProfileMode = OMX_FALSE,
        .bExtradata = OMX_FALSE,
        .nIDRPeriod = 0,
        .nHierPNumLayers = 0,
        .nHECInterval = 0,
        .nTimeIncRes = 30,
        .bEnableShortHeader = OMX_FALSE,
        .bCABAC = OMX_TRUE,
        .nDeblocker = 1,
        .id = 0,
        .cancel_flag = 1,
        .type = 0,
        .quincunx_sampling_flag = 0,
        .content_interpretation_type = 0,
        .spatial_flipping_flag = 0,
        .frame0_flipped_flag = 0,
        .field_views_flag = 0,
        .current_frame_is_frame0_flag = 0,
        .frame0_self_contained_flag = 0,
        .frame1_self_contained_flag = 0,
        .frame0_grid_position_x = 0,
        .frame0_grid_position_y = 0,
        .frame1_grid_position_x = 0,
        .frame1_grid_position_y = 0,
        .reserved_byte = 0,
        .repetition_period = 0,
        .extension_flag = 0,
        .nLTRMode = 0,
        .nLTRCount = 0,
        .nLTRPeriod = 0,
    };

    /* Encoder Vars */
    OMX_HANDLETYPE hEncoder_ = NULL;
    OMX_S32 inputBuffersCount_;
    OMX_S32 outputBuffersCount_;
    OMX_S32 inputBufferSize_;
    OMX_S32 outputBufferSize_;
    omxa::BufferPoolPtr input_;
    omxa::BufferPoolPtr output_;

    omxa::OmxSinkPtr  outputComponent_;

    /* Camera Vars */
    omxa::OmxDrainPtr inputComponent_;
    omxa::CameraComponent cameraComponent_;
    camera::CameraParams mCameraParams;
    std::shared_ptr<camera::ICameraDevice> camera_;

    omxa::CameraComponent::CameraStream stream_ ;

    void initEncoder();

    virtual int configureCamera() = 0;
    int initializeCamera();

    int initializeEncoder();
    virtual int configureEncoder();

    virtual int startPlaying() = 0;
    virtual void stopPlaying() = 0;

    void finalizeEncoder();

    int allocOMXBuffers();

    omxa::EncoderComponent encoderComponent_;

    std::atomic_bool ready_ = {false};   /* perform atomic write to indicate the session is ready
                           ** to process the frames from encoder. */

    static OMX_ERRORTYPE EventCallback(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_IN OMX_PTR pAppData,
                                       OMX_IN OMX_EVENTTYPE eEvent,
                                       OMX_IN OMX_U32 nData1,
                                       OMX_IN OMX_U32 nData2,
                                       OMX_IN OMX_PTR pEventData) {
        class VSession* me = static_cast<class VSession*>(pAppData);

        QCAM_INFO("eEvent : 0x%x, nData1 : 0x%x, nData2 : 0x%x", eEvent, nData1, nData2);

        if (OMX_EventCmdComplete == eEvent) {
            if (OMX_CommandStateSet == (OMX_COMMANDTYPE)nData1) {
                me->encoderComponent_.set((OMX_STATETYPE)nData2);
            }
        }
        return OMX_ErrorNone;
    }

    static OMX_ERRORTYPE EmptyDoneCallback(OMX_IN OMX_HANDLETYPE hComponent,
                                           OMX_IN OMX_PTR pAppData,
                                           OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
        class VSession* me = static_cast<class VSession*>(pAppData);
        (void)me;

        if (me->ready_) {
            me->inputComponent_->emptyBufferDone(hComponent, pBuffer);
        }

        return OMX_ErrorNone;
    }

    static OMX_ERRORTYPE FillDoneCallback(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_PTR pAppData,
                                          OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
        class VSession* me = static_cast<class VSession*>(pAppData);
        (void)me;
        OMX_ERRORTYPE omxErr = OMX_ErrorIncorrectStateOperation;

        if (me->ready_) {
            omxErr = me->outputComponent_->emptyBuffer(pBuffer);
        }
        return omxErr;
    }

    virtual int initSink() = 0;

public:
    VSession() {}
    virtual ~VSession() { (void) stop(); }

    virtual int start();
    virtual int stop();
    virtual void setConfig(const SessionConfig& config) {
        mConfig = config;
    }
    virtual void setConfig(const H264Config& config) {
    }
    virtual int setConfig(JSONParser& js) {
        JSONID res_arr_val;

        if (JSONPARSER_SUCCESS == JSONParser_Lookup(&js, 0, "resolution", 0,
                                                    &res_arr_val)) {
            JSONID width_val;
            JSONID height_val;
            unsigned int width;
            unsigned int height;

            /* TODO: error checking */
            JSONParser_ArrayLookup(&js, res_arr_val, 0, &width_val);
            JSONParser_ArrayLookup(&js, res_arr_val, 1, &height_val);

            JSONParser_GetUInt(&js, width_val, &width);
            JSONParser_GetUInt(&js, height_val, &height);

            mConfig.width = width;
            mConfig.height = height;
        }

        return 0;
    }
};

/* Find and select the camera to use */
static int getCameraByFunction(int cf)
{
    int camId = -1;
    int numCameras = camera::getNumberOfCameras();
    struct camera::CameraInfo info;

    for (int i = 0 ; i < numCameras ; i++) {
        camera::getCameraInfo(i, info);

        if (cf == info.func ) {
            camId = i;
            break;
        }
    }

    return camId;
}

#define YUV420_BUF_SIZE(w, h) ((w) * (h) * 3/2)

int VSession::initializeCamera()
{
    int rc = EXIT_SUCCESS;
    int camId;

    camId = getCameraByFunction(0);   /* TODO: configure camera identification */
    if (camId == -1 ) {
        QCAM_ERR("Failed to find camera by function: %d", 0);
        THROW(rc, ENOENT);
    }

    camera_ = CameraFactory::get(camId);
    if (!camera_) {
        THROW(rc, EIO);
    }

    TRY(rc, mCameraParams.init(camera_.get()));

    cameraComponent_.init(camera_.get());

    CATCH(rc) {QCAM_ERR("failed to initializeCamera() : %d", rc);}
    return rc;
}

int VSession::initializeEncoder()
{
    int rc = EXIT_SUCCESS;
    OMX_ERRORTYPE omxError;
    static OMX_CALLBACKTYPE callbacks = {
        .EventHandler = VSession::EventCallback,
        .EmptyBufferDone = VSession::EmptyDoneCallback,
        .FillBufferDone = VSession::FillDoneCallback,
    };

    omxError = OMX_GetHandle(&hEncoder_, (OMX_STRING)"OMX.qcom.video.encoder.avc",
                             this, &callbacks);
    if (OMX_ErrorNone != omxError) {
        QCAM_ERR("Failed to find video.encoder.h263: 0x%x", omxError);
        THROW(rc, ENXIO);
    }

    {   // extension "OMX.google.android.index.storeMetaDataInBuffers"
        android::StoreMetaDataInBuffersParams meta_mode_param = {
            .nSize = sizeof(android::StoreMetaDataInBuffersParams),
            .nVersion = 0x00000101,
            .nPortIndex = PORT_INDEX_IN,
            .bStoreMetaData = OMX_TRUE,
        };
        omxError = OMX_SetParameter(
            hEncoder_,
            (OMX_INDEXTYPE)OMX_QcomIndexParamVideoMetaBufferMode,
            (OMX_PTR)&meta_mode_param);
    }
    if (OMX_ErrorNone != omxError) {
        QCAM_ERR("Failed to enable Meta data mode: 0x%x", omxError);
        THROW(rc, ENXIO);
    }

    TRY(rc, encoderComponent_.init(hEncoder_, OMX_StateInvalid));

    CATCH(rc) {QCAM_ERR("failed to initializeEncoder : %d", rc);}

    return rc;
}

int VSession::configureEncoder()
{
    int rc = EXIT_SUCCESS;
    OMX_ERRORTYPE omxError;

    omxError = omx::video::encoder::Configure(
        hEncoder_, OMX_VIDEO_CodingAVC,
        inputBuffersCount_, outputBuffersCount_,
        inputBufferSize_, outputBufferSize_,
        encoderConfig_);
    if (omxError != OMX_ErrorNone) {
        QCAM_ERR("Failed to find configure video.encoder.h263: 0x%x", omxError);
        THROW(rc, ENXIO);
    }
    QCAM_INFO("Encoder configured.");

    CATCH(rc) {QCAM_ERR("failed to configureEncoder : %d", rc);}
    return rc;
}

int VSession::allocOMXBuffers()
{
    int rc = EXIT_SUCCESS;
    OMX_ERRORTYPE omxError;

    omxError = omxa::BufferPool::create(
        hEncoder_, PORT_INDEX_IN, inputBuffersCount_, inputBufferSize_, &input_);
    if (OMX_ErrorNone != omxError) {
        QCAM_ERR("Failed to omxa::BufferPool::create(Input): 0x%x", omxError);
        THROW(rc, ENXIO);
    }

    omxError = omxa::BufferPool::create(
        hEncoder_, PORT_INDEX_OUT, outputBuffersCount_, outputBufferSize_,
        &output_);
    if (OMX_ErrorNone != omxError) {
        QCAM_ERR("Failed to omxa::BufferPool::create(Output): 0x%x", omxError);
        THROW(rc, ENXIO);
    }

    QCAM_INFO("buffers input: 0x%x, output: 0x%x",
              (uint32_t)inputBuffersCount_,
              (uint32_t)outputBuffersCount_);

    CATCH(rc) {QCAM_ERR("failed to allocOMXBuffers() : %d", rc);}
    return rc;
}

/**
 * @brief session goes in to start.
 *
 *
 * @return int 0 for success, non-zero for failure
 */
int VSession::start()
{
    int rc = EXIT_SUCCESS;

    // Initialize camera
    TRY(rc, initializeCamera());

    // Configure camera
    TRY(rc, configureCamera());

    TRY(rc, initializeEncoder());

    encoderConfig_.nFrameWidth = mConfig.width;
    encoderConfig_.nFrameHeight = mConfig.height;
    encoderConfig_.nOutputFrameWidth = mConfig.width;
    encoderConfig_.nOutputFrameHeight = mConfig.height;

    TRY(rc, configureEncoder());

    TRY(rc, encoderComponent_.enter(OMX_StateIdle));

    TRY(rc, allocOMXBuffers());

    encoderComponent_.wait_until(OMX_StateIdle);   /** todo: support for EINTR */

    TRY(rc, initSink());

    TRY(rc, encoderComponent_.enter_and_wait_until(OMX_StateExecuting));

    ready_ = true;

    /* submit output buffers to encoder for filling */
    for (OMX_BUFFERHEADERTYPE* pBuffer : *output_) {
        pBuffer->nFlags = 0;
        TRY(rc, OMX_FillThisBuffer(hEncoder_, pBuffer));
    }

    /* Attach the camera to encoder along with the buffers for input */
    TRY(rc, cameraComponent_.openOMXDrain(stream_, hEncoder_, input_,
                                          &inputComponent_));

    QCAM_INFO("Session[%d] start", (int)stream_);

    /* start the source */
    startPlaying();

    CATCH(rc) {
        QCAM_ERR("QCAMVID2::VSession::start() failed");
    }

    return rc;
}

int VSession::stop()
{
    int rc = EXIT_SUCCESS;

    ready_ = false;

    if (camera_ != nullptr) {
        QCAM_INFO("Session[%d] stop", (int)stream_);
        encoderComponent_.enter_and_wait_until(OMX_StateIdle);

        /* Tear down the encoder and camera components */
        encoderComponent_.reset();
        cameraComponent_.reset();

        /* close all buffers */
        input_.reset();
        output_.reset();

        /* close the drain from camera to encoder */
        inputComponent_.reset();
        /* close the sink from encoder to file */
        outputComponent_.reset();

        /* stop the source */
        stopPlaying();

        OMX_FreeHandle(hEncoder_);
        hEncoder_ = NULL;

        camera_.reset();

        QCAM_INFO("Session[%d] Capture Finished.", (int) stream_);
    }

    return rc;
}

class RecordingSession : public VSession {
    /* File Vars */
    omxa::FileComponent file_;
public:
    RecordingSession() {
        stream_ = omxa::CameraComponent::STREAM_VIDEO;
    }

    virtual int configureCamera() {
        camera::ImageSize frame_size;
        int rc = EXIT_SUCCESS;

        frame_size.width = mConfig.width;
        frame_size.height = mConfig.height;
        mCameraParams.setVideoSize(frame_size);
        QCAM_INFO("set video size:%dx%d, %d bytes/frame \n",
                  frame_size.width, frame_size.height,
                  YUV420_BUF_SIZE(frame_size.width, frame_size.height));

        rc = mCameraParams.commit();
        if (rc != 0) {
            QCAM_ERR("failed to commit camera parameters\n");
        }
        return rc;
    }

    virtual int initSink() {
        int rc;

        TRY(rc, file_.init(""));
        TRY(rc, file_.openOMXSink(hEncoder_, &outputComponent_));
        CATCH(rc) {}

        return rc;
    }

    virtual int startPlaying() {
        QCAM_INFO("start recording");
        return camera_->startRecording();
    }

    virtual void stopPlaying() {
        QCAM_INFO("Stop recording....");
        camera_->stopRecording();
    }
};

class PreviewSession : public VSession, public omxa::IPreviewComp {

    omxa::PreviewComponentPtr preview_;
    std::shared_ptr<FramedSource> inputSource_;
    omxa::PreviewParameters params_;

public:
    PreviewSession() {
        stream_ = omxa::CameraComponent::STREAM_PREVIEW;
    }

    virtual int openFramedSource(
        UsageEnvironment& env, std::shared_ptr<FramedSource>& source_out) {
        return preview_->openDiscreteH264Source(env, source_out);
    }

    virtual int configureCamera() {
        camera::ImageSize frame_size;
        int rc = EXIT_SUCCESS;

        frame_size.width = mConfig.width;
        frame_size.height = mConfig.height;
        mCameraParams.setPreviewSize(frame_size);
        QCAM_INFO("set preview size:%dx%d, %d bytes/frame \n",
                  frame_size.width, frame_size.height,
                  YUV420_BUF_SIZE(frame_size.width, frame_size.height));

        /* change the output format for the preview to nv12-venus */
        mCameraParams.set(std::string("preview-format"),
                          std::string("nv12-venus"));

        rc = mCameraParams.commit();
        if (rc != 0) {
            QCAM_ERR("failed to commit camera parameters\n");
        }
        return rc;
    }

    virtual int initSink() {
        int rc;

        TRY(rc, omxa::PreviewComponent::create(params_, &preview_));
        TRY(rc, preview_->openOMXSink(hEncoder_, &outputComponent_));

        CATCH(rc) {}
        return rc;
    }

    virtual int configureEncoder() {
        encoderConfig_.eCodecProfile = omx::video::encoder::AVCProfileBaseline;
        encoderConfig_.nBitrate      = 1024 * 1024; // TODO:  use params_
        encoderConfig_.nIntraPeriod  = 6; // TODO:  use params_

        return VSession::configureEncoder();
    }

    virtual int startPlaying() {
        QCAM_INFO("start previewing");
        return camera_->startPreview();
    }

    virtual void stopPlaying() {
        QCAM_INFO("Stop previewing");
        camera_->stopPreview();
    }
};

/**
 Create an instance of the session type.
 @param sessionType*
 @return ISession* [out] not NULL if succeeded in creating the instance.
 **/
static ISession* createSession(SessionType sessionType)
{
    /* TODO: load from config file */
    if (QCAM_SESSION_RECORDING == sessionType) {
        return new RecordingSession();
    }

    if (QCAM_SESSION_RTP == sessionType) {
        return new PreviewSession();
    }

    return NULL;
}

std::map<SessionType, std::weak_ptr<ISession>> SessionMgr::sessions_;

std::shared_ptr<ISession> SessionMgr::get(SessionType st)
{
    std::shared_ptr<ISession> s = nullptr;

    auto i = sessions_.find(st);
    if (i != sessions_.end()) {   /* found */
        s = i->second.lock();
    }
    else {
        ISession* nsess = createSession(st);

        if (NULL != nsess) {
            QCAM_INFO("New Session type: %d\n", (int)st);

            /* install in the shared ptr. override the default delete */
            s.reset(nsess, [](ISession* p) {
                auto j = sessions_.begin();
                while (j != sessions_.end()) {
                    if (j->second.expired()) {
                        sessions_.erase(j++);
                    }
                    else {
                        j++;
                    }
                }
                delete p;
            });
            sessions_[st] = s;
        }
    }
    return s;
}
}

