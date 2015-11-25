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
#ifndef __FPV_SERVER_H__
#define __FPV_SERVER_H__

#include <thread>
#include <string>
#include <future>

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

namespace camerad
{

/**
 FpvServer provides a hosting environment and a run-time thread for the liveMedia
 RTSP service. FpvServer::start() wil start the environment and optionally binds
 the network listener to a given iface.

 After the server
**/
class FpvServer
{
public:
	FpvServer();
	virtual ~FpvServer(){};

    /**
     Start the fpv server run-time-environment. This blocks until the service has
     been started sufficiently for other methods of this class.

     Optionally bind to a given network interface. Useful
     when system has multiple network interfaces. By default the server uses
     INADDR_ANY.

     @codeexample
     @code
     server->start("wlan0");
     @endcode

     @param iface_name : the name of interface. For example the utility command
       `ifconfig` will list the available network interfaces.

     @return int
     **/
	int start(const std::string& iface_name = "");

    /**
     Stop the FPV (i.e RTSP services). This blocks for the rte to shutdown.

     @return int : 0 when successfully stopped the service.
     **/
	int stop();

    /**
     Add the given session for streaming over RTP. This method basically forwards
     the add request to the FpvSerer. Actual processing will occur asynchronously.
     When successfully added, the session becomes visible over RTSP.

     @param [in] uid : unique request id by the client. Response must include
            this identifier.
     @param [in] params : json string with request parameters.
     @param [in] param_siz : size of the string at params.

     @return int
     **/
    int addSession(unsigned int uid, const char* params, int param_siz);

    UsageEnvironment& env() {
        return *env_;
    }

private:

    std::promise<bool> start_promise_;

    char stopflag_;
    int  port_;
    std::string net_iface_;
    std::thread task_;      /**< a hosting thread */

    EventTriggerId signal_; /**< used to resume asynchronous operations */

    /** LiveMedia parameters */
    TaskScheduler* scheduler_;
    UsageEnvironment* env_;
    RTSPServer* rtsp_;

    void doRTSP();
    void doSession();
    static void dispatchSignal(FpvServer* me);
};
}

#endif /* !__FPV_SERVER_H__ */