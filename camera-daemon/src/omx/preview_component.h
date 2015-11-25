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
#ifndef __OMXA_PREVIEW_COMPONENT_H__
#define __OMXA_PREVIEW_COMPONENT_H__

#include "omx/omx_sink.h"

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

namespace omxa {

// The following class can be used to define specific encoder parameters
class PreviewParameters {
};

class PreviewComponent;
typedef std::shared_ptr<PreviewComponent> PreviewComponentPtr;

class IPreviewComp
{
public:
    IPreviewComp(){};
    virtual ~IPreviewComp(){};
    virtual int openFramedSource(
        UsageEnvironment& env, std::shared_ptr<FramedSource>& source_out) = 0;
};

/**
 * A bridge between rtp stream and OpenMax h264 video encoder. In this component 
 * there are two ports. An output port - sink for writing from video encoder. 
 * An input port - FramedSource for retrieving the H264 data. Input port is 
 * designed to be compatible with Live555 source interface.
 **/
class PreviewComponent {
protected:
    PreviewComponent() {}
public:
    static int create(PreviewParameters& params, PreviewComponentPtr* out);

    virtual ~PreviewComponent() {}
    virtual int openOMXSink(OMX_HANDLETYPE hComponent, OmxSinkPtr* ppout) = 0;

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
    virtual int openDiscreteH264Source(UsageEnvironment& env,
        std::shared_ptr<FramedSource>& source_out) = 0;

    /**
     * This will return a FramedSource instances for use with
     * H264VideoStreamFramer as a live source.
     *
     * @param env : live555 Environment
     * @param source_out : FramedSource instance. Follow the shared_ptr ownership
     *           policy with this object.
     *
     * @return int : 0 on success
     **/
    virtual int openH264Source(UsageEnvironment& env,
        std::shared_ptr<FramedSource>& source_out) = 0;
};
}
#endif /* !__OMXA_PREVIEW_COMPONENT_H__ */

