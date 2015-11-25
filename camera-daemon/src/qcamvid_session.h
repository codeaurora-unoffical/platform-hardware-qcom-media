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
#ifndef __QCAMVID_SESSION_H__
#define __QCAMVID_SESSION_H__
#include <string>
#include "json/json_parser.h"

namespace camerad
{

/** Video session config, default values are used for initialization. */
struct SessionConfig
{
    int width = 1280;   /**< width of the video */
    int height = 720;   /**< height of the video */
    int fps = 24;       /**< frames per second of the video */
    std::string focusMode = "off";   /**< focus setting; supported values : TODO */
};

/**
 ISession is a simple interface for building a video session. 
  
 A sesion is an end-to-end path of video from live source in this 
 case a camera to a final sink such as an rtp sink or a file sink. 
 A session builder will add several omxa components to complete 
 the path. This interface provides control methods such as start() 
 and stop() to facilitate user control. 
 **/
class ISession {
public:
    ISession() {}
    virtual ~ISession() {}

    /**
     Start the video session
     @return int
     **/
    virtual int start() = 0;

    /**
     Stop the video session
     @return int
     **/
    virtual int stop() = 0;

    /**
     Set the session configuration
     @param config
     **/
    virtual void setConfig(const SessionConfig& config) = 0;

    /**
     Parse and extract the session configuration.
     @param js json parser instance
     **/
    virtual int setConfig(JSONParser& js) = 0;
};

enum SessionType {
    QCAM_SESSION_RTP,       /**< A session for streaming live, uses preview
                                 stream from camera */
    QCAM_SESSION_RECORDING, /**< Session for recording to filesystem, uses
                                 a dedicated video stream from camera */
};

/**
 Create an instance of the session type.
 @param sessionType*
 @return ISession* [out] not NULL if succeeded in creating the instance.
 **/
ISession* createSession(SessionType sessionType);
}
#endif /* !__QCAMVID_SESSION_H__ */

