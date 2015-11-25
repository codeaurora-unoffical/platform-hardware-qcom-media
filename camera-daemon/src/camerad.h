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
#ifndef __CAMERAD_H__
#define __CAMERAD_H__

#include <string>
#include "camera.h"
#include "camera_parameters.h"
#include "qcamdaemon.h"
#include "json/json_parser.h"

namespace camerad
{

enum DaemonLoglevel {
    QCAM_LOG_SILENT = 0,
    QCAM_LOG_ERROR = 1,
    QCAM_LOG_INFO = 2,
    QCAM_LOG_DEBUG = 3,
    QCAM_LOG_MAX,
};

/* Daemon config, default values are used for initialization. */
struct DaemonConfig
{
    bool quit = false;
    DaemonLoglevel logLevel = QCAM_LOG_ERROR;
    bool daemon = true;
};

#define PID_FILE "/var/run/camerad.pid"
#define CONFIG_FILE "camerad.json"

int daemonMain(const DaemonConfig& cfg);

/**
 Get the configuration of the camera identified by the indx.

 @param indx : index of the camera connected to device.
 @param jp : initialize parser to the json object.

 @return int
 **/
int cfgGetCamera(int indx, JSONParser& jp);

}
#endif

