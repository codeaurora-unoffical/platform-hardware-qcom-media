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

/** Manage an H264 RTP streaming subsession. */
namespace camerad
{
fpvH264OnDemandMediaSubsession::fpvH264OnDemandMediaSubsession(
    UsageEnvironment& env, const char* params, int param_siz)
: OnDemandServerMediaSubsession(env, True)
{
    m_pSDPLine = NULL;
    m_pDummyRTPSink = NULL;
    m_done = 0;

    /*TODO: retain the params for later creation of the source */
}

fpvH264OnDemandMediaSubsession::~fpvH264OnDemandMediaSubsession(void)
{
    if (m_pSDPLine)
        free(m_pSDPLine);
}

fpvH264OnDemandMediaSubsession* fpvH264OnDemandMediaSubsession::createNew(
    UsageEnvironment& env, const char* params, int param_siz)
{
    return new fpvH264OnDemandMediaSubsession(env, params, param_siz);
}

/** Create the H264 video stream source and start the RTP session. */
FramedSource* fpvH264OnDemandMediaSubsession::createNewStreamSource(
    unsigned clientSessionId, unsigned& estBitrate)
{
    rtpSession_ = createSession(QCAM_SESSION_RTP);

    rtpSession_->start();

    estBitrate = 90000;

    /* todo: revisit for a better architecture */
    omxa::IPreviewComp* comp = dynamic_cast<omxa::IPreviewComp*>(rtpSession_);
    if (NULL == comp) {
        return NULL;
    }
    comp->openFramedSource(envir(), src_);
    return H264VideoStreamDiscreteFramer::createNew(envir(), src_.get());
}

void fpvH264OnDemandMediaSubsession::closeStreamSource(FramedSource* inputSource)
{
    rtpSession_->stop();
}

/** Create a new RTP sink that is used by the encoder to provide
 *  frames for streaming. */
RTPSink * fpvH264OnDemandMediaSubsession::createNewRTPSink(Groupsock * rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource * inputSource)
{
    H264VideoRTPSink *RTPSink = H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    OutPacketBuffer::increaseMaxSizeTo(500000); // allow for some possibly large H.264 frames
    RTPSink->setPacketSizes(7, 1456);
    return RTPSink;
}

char const * fpvH264OnDemandMediaSubsession::getAuxSDPLine(RTPSink * rtpSink, FramedSource * inputSource)
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

void fpvH264OnDemandMediaSubsession::afterPlayingDummy(void * ptr)
{
    fpvH264OnDemandMediaSubsession * mysess = (fpvH264OnDemandMediaSubsession *) ptr;
    mysess->afterPlayingDummy1();
}

void fpvH264OnDemandMediaSubsession::afterPlayingDummy1()
{
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    m_done = 0xff;
}

void fpvH264OnDemandMediaSubsession::chkForAuxSDPLine(void * ptr)
{
    fpvH264OnDemandMediaSubsession * mysess = (fpvH264OnDemandMediaSubsession *)ptr;
    mysess->chkForAuxSDPLine1();
}

void fpvH264OnDemandMediaSubsession::chkForAuxSDPLine1()
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
