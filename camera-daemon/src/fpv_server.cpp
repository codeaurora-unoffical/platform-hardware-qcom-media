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

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include "fpv_server.h"
#include "fpv_h264.h"
#include "qcamvid_log.h"

namespace camerad
{

FpvServer::FpvServer()
{
    stopflag_ = 0;
    port_ = 554;
}

/**
 Bind server to receive connections on given network interface. This is used
 when the system has multiple network interfaces. By default the server uses
 INADDR_ANY. This must be setup before the start() of the server.

 @param iface_name : the name of interface. For example the utility command
   `ifconfig` will list the available network interfaces.

 @return int : ENOENT if the interface isn't up. 0 on success.
 **/
static int useNetInterface(const std::string& iface_name)
{
    struct ifaddrs* ifAddrStruct = NULL;

    for (getifaddrs(&ifAddrStruct); NULL != ifAddrStruct;
          ifAddrStruct = ifAddrStruct->ifa_next) {
        if (AF_INET == ifAddrStruct->ifa_addr->sa_family
            && 0 == strcmp(iface_name.c_str(), ifAddrStruct->ifa_name)) {
            struct sockaddr_in* sock_addr
                = (struct sockaddr_in*)ifAddrStruct->ifa_addr;

            /* assign this interface address to the  global variable
               in GroupsockHelper.hh */
            ReceivingInterfaceAddr = sock_addr->sin_addr.s_addr;
            return 0;
        }
    }

    return ENOENT;
}

void FpvServer::dispatchSignal(FpvServer* me) {
    me->doSession();
}

void FpvServer::doRTSP()
{
    (void)useNetInterface(net_iface_);

    scheduler_ = BasicTaskScheduler::createNew();
    env_ = BasicUsageEnvironment::createNew(*scheduler_);

    rtsp_ = RTSPServer::createNew(*env_, port_, NULL, 0);

    signal_ = scheduler_->createEventTrigger((TaskFunc*)FpvServer::dispatchSignal);

    start_promise_.set_value(true);

    scheduler_->doEventLoop(&stopflag_);
}

int FpvServer::start(const std::string& iface_name)
{
    if (task_.joinable()) {
        return EALREADY;
    }
    std::future<bool> start_ready = start_promise_.get_future();

    net_iface_ = iface_name;   /* use this network interface */

    QCAM_INFO("start fpv server");
    task_ = std::thread(&FpvServer::doRTSP, this);

    start_ready.wait();

    return 0;
}

int FpvServer::stop()
{
    stopflag_ = 1;
    if (task_.joinable()) {
        task_.join();
    }
    return 0;
}

int FpvServer::addSession(unsigned int uid, const char* param, int param_siz)
{
    std::unique_lock<std::mutex> lk(lock_);
    if (0 == rtsp_) {
        return ENOSR;
    }

    /** push the params in to a queue */
    requests_.push(Request(uid, param, param_siz));

    if (NULL != scheduler_) {
        scheduler_->triggerEvent(signal_, this);
    }

    return 0;
}

void FpvServer::doSession()
{
    std::unique_lock<std::mutex> lk(lock_);
    Request req;

    while (!requests_.empty()) {

        req = requests_.front();
        requests_.pop();

        /* make a media session.
           TODO: process the params for the session name. */
        ServerMediaSession* sms = ServerMediaSession::createNew(
            *env_, "fpvview", 0, "session stream for fpv", true);

        /* forward params to video subsession */
        fpvH264* h264subsession = fpvH264::createNew(
                *env_, req.param_.c_str(), req.param_.length());

        sms->addSubsession(h264subsession);

        rtsp_->addServerMediaSession(sms);
    }

    return;
}

}
