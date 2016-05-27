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
#include "fpv_h264.h"

/** Manage a H264 RTP streaming subsession. */
namespace camerad
{
fpvH264::fpvH264(
    UsageEnvironment& env, const char* params, int param_siz)
: OnDemandServerMediaSubsession(env, True)
{
    m_pSDPLine = NULL;
    m_pDummyRTPSink = NULL;
    m_done = 0;

    /* retain the params for later creation of the source */
    params_ = std::string(params, param_siz);
}

fpvH264::~fpvH264(void)
{
    if (m_pSDPLine) {
        free(m_pSDPLine);
        m_pSDPLine = NULL;
    }
}

fpvH264* fpvH264::createNew(UsageEnvironment& env, const char* params,
                            int param_siz)
{
    return new fpvH264(env, params, param_siz);
}

/** Create the H264 video stream source and start the RTP session. */
FramedSource* fpvH264::createNewStreamSource(
    unsigned clientSessionId, unsigned& estBitrate)
{
    rtpSession_ = SessionMgr::get(QCAM_SESSION_RTP);

    if (params_.length()) {
        JSONParser js;
        JSONType jt;

        JSONParser_Ctor(&js, params_.c_str(), params_.length());
        if (JSONPARSER_SUCCESS == JSONParser_GetType(&js, 0, &jt)
            && JSONObject == jt) {
            (void)rtpSession_->setConfig(js);
        }
    }

    int rc = rtpSession_->start();
    if (rc != EXIT_SUCCESS) {
        return NULL;
    }

    estBitrate = 90000;

    /* todo: revisit for a better architecture */
    omxa::IPreviewComp* comp = dynamic_cast<omxa::IPreviewComp*>(rtpSession_.get());
    if (NULL == comp) {
        return NULL;
    }
    comp->openFramedSource(envir(), src_);
    return H264VideoStreamDiscreteFramer::createNew(envir(), src_.get());
}

void fpvH264::closeStreamSource(FramedSource* inputSource)
{
    rtpSession_->stop();
}

/** Create a new RTP sink that is used by the encoder to provide
 *  frames for streaming. */
RTPSink * fpvH264::createNewRTPSink(Groupsock * rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource * inputSource)
{
    H264VideoRTPSink *RTPSink = H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    OutPacketBuffer::increaseMaxSizeTo(500000); // allow for some possibly large H.264 frames
    RTPSink->setPacketSizes(7, 1456);
    return RTPSink;
}

char const * fpvH264::getAuxSDPLine(RTPSink * rtpSink, FramedSource * inputSource)
{
    if (m_pSDPLine != NULL)  {
        return m_pSDPLine;
    }

    if (m_pDummyRTPSink == NULL) {
        m_pDummyRTPSink = rtpSink;
        m_pDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);
        chkForAuxSDPLine(this);
    }

    envir().taskScheduler().doEventLoop(&m_done);

    return m_pSDPLine;
}

void fpvH264::afterPlayingDummy(void * ptr)
{
    fpvH264 * mysess = (fpvH264 *) ptr;
    mysess->afterPlayingDummy1();
}

void fpvH264::afterPlayingDummy1()
{
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    m_done = 0xff;
}

void fpvH264::chkForAuxSDPLine(void * ptr)
{
    fpvH264 * mysess = (fpvH264 *)ptr;
    mysess->chkForAuxSDPLine1();
}

void fpvH264::chkForAuxSDPLine1()
{
    char const* sdp;

    if (m_pSDPLine != NULL) {
        m_done = 0xff;
    }
    else if (m_pDummyRTPSink != NULL && (sdp = m_pDummyRTPSink->auxSDPLine()) != NULL) {
        m_pSDPLine = strDup(sdp);
        m_pDummyRTPSink = NULL;
        m_done = 0xff;
    }
    else {
        int to_delay_us = 34000;
        nextTask() = envir().taskScheduler().scheduleDelayedTask(to_delay_us, (TaskFunc*)chkForAuxSDPLine, this);
    }
}
}
