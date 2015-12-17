/* Copyright (c) 2015-16, The Linux Foundation. All rights reserved.
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
 *  Copyright (C) 2007-2010 by the JSON-RPC Working Group
 *
 *  This document and translations of it may be used to implement JSON-RPC, it
 *  may be copied and furnished to others, and derivative works that comment on
 *  or otherwise explain it or assist in its implementation may be prepared,
 *  copied, published and distributed, in whole or in part, without restriction
 *  of any kind, provided that the above copyright notice and this paragraph are
 *  included on all such copies and derivative works. However, this document
 *  itself may not bemodified in any way.
 *
 *  The limited permissions granted above are perpetual and will not be revoked.
 *  This document and the information contained herein is provided "AS IS" and
 *  ALL WARRANTIES, EXPRESS OR IMPLIED are DISCLAIMED, INCLUDING BUT NOT LIMITED
 *  TO ANY WARRANTY THAT THE USE OF THE INFORMATION HEREIN WILL NOT INFRINGE ANY
 *  RIGHTS OR ANY IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A
 *  PARTICULAR PURPOSE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string>
#include <map>

#include "camerad.h"
#include "camerad_util.h"
#include "qcamvid_log.h"
#include "qcamvid_session.h"
#include "fpv_server.h"

#include "json/json_parser.h"

#include "js_invoke.h"

#define METHOD_NAME_LEN_MAX 128

namespace camerad
{

#define CMD_SIZE 512

class QCamDaemon {

    uint8_t cmd_buf_[CMD_SIZE+1];   /* incoming messages are read in to this buffer.
                                       todo: separate this buffer by socket. */
    size_t  cmd_buf_occupied_ = 0;  /* number of bytes occupied in the buffer */
    int sock_ = -1;
    int current_client_ = -1;
    std::shared_ptr<ISession> recSession_ = NULL;   /* video recording session */
    FpvServer* fpv_ = NULL;         /* fpv task */

    DaemonConfig cfg_;
    bool stop_ = false;

    int initSock(int port);

    typedef void (QCamDaemon::*ReqFunctor)(unsigned int uid, const char* params,
                                int param_siz);

    std::map<std::string, ReqFunctor> requests_;

    void camera_recording_start(unsigned int uid, const char* params,
                                int param_siz) {
        int rc = 0;

        /* TODO: support for mapping session and camera id */
        if (param_siz) {
            JSONParser js;
            JSONType jt;

            JSONParser_Ctor(&js, params, param_siz);
            if (JSONPARSER_SUCCESS == JSONParser_GetType(&js, 0, &jt)
                && JSONObject == jt) {
                TRY(rc, recSession_->setConfig(js));
            }
        }

        TRY(rc, recSession_->start());

        CATCH(rc) {}

        jsResult_Send(current_client_, uid, rc);
    }

    void camera_recording_stop(unsigned int uid, const char* params,
                               int param_siz) {
        jsResult_Send(current_client_, uid, recSession_->stop());
    }

    void camera_rtsp_start(unsigned int uid, const char* params, int param_siz) {
        if (0 == fpv_) {
            fpv_ = new FpvServer();
            fpv_->start("wlan0");   /* TODO: get the iface name from config */
        }
        else {
            /* respond with error */
            jsResult_Send(current_client_, uid, EALREADY);
            return;
        }

        /* TODO: error checking */
        fpv_->addSession(uid, params, param_siz);
    }

    void camera_rtsp_stop(unsigned int uid, const char* params, int param_siz) {
        fpv_->stop();
        delete fpv_; fpv_ = 0;
    }

public:
    int init(const DaemonConfig& cfg, int port) {
        int rc = initSock(port);
        if (0 == rc) {
            cfg_ = cfg;
            recSession_ = SessionMgr::get(QCAM_SESSION_RECORDING);
            if (NULL == recSession_) {
                rc = ENOMEM;
            }

            requests_.insert(std::make_pair("camera.recording.start", &QCamDaemon::camera_recording_start));
            requests_.insert(std::make_pair("camera.recording.stop",  &QCamDaemon::camera_recording_stop));
            requests_.insert(std::make_pair("camera.rtsp.start",      &QCamDaemon::camera_rtsp_start));
            requests_.insert(std::make_pair("camera.rtsp.stop",       &QCamDaemon::camera_rtsp_stop));
        }
        return rc;
    }

    void final(void) {
        recSession_.reset();
        if (-1 != sock_) {close(sock_); sock_ = -1; }
    }

    int run(void);

    int handleInput(int fd);

    void handleRequest(jsCall& call) {
        JSONParser js;
        unsigned int uId;                       /* request id */
        char method[METHOD_NAME_LEN_MAX];       /* request name */

        JSONParser_Ctor(&js, call.msg_, call.msg_siz_);
        if (JSONPARSER_SUCCESS == JSONParser_GetString(&js, call.method_, method,
                                                       sizeof(method), NULL)
            && JSONPARSER_SUCCESS == JSONParser_GetUInt(&js, call.id_, &uId)) {
            ReqFunctor rqf;
            const char* p;
            int n;
            JSONID params;
            std::string params_val;

            if (JSONPARSER_SUCCESS == JSONParser_Lookup(&js, 0, "params", 0,
                                                        &params)) {
                JSONParser_GetJSON(&js, params, &p, &n);
                params_val =  std::string(p,n);
            }

            rqf = requests_[method];

            if (0 != rqf) {
                (this->*rqf)(uId, params_val.c_str(), params_val.length());
            }
            else {
                /** TODO: respond with error */
            }
        }
        else {
            jsResult_Send(current_client_, 0xFFFFFFFF, EBADMSG);
        }
    }

    void handleMessage(uint8_t* arg, size_t arg_len, size_t& processed) {
        JSONParser j;
        const char* p;
        int n;
        jsCall call;

        JSONParser_Ctor(&j, (const char*)arg, (int)arg_len);

        for (processed = 0;
              processed < arg_len
              && JSONPARSER_SUCCESS == JSONParser_GetJSON(&j, processed, &p, &n);
              processed += n) {
            (void)call.ctor(p, n);

            QCAM_INFO("MSG : %s", std::string(p, n).c_str());
            //call.dump();

            /* todo: currently handling requests only, add support for other
               message types */
            if (jsCall::Request == call.type_) {
                handleRequest(call);
            }
        }
    }
};

int QCamDaemon::initSock(int port)
{
    int sock = -1, ret = 0;
    struct sockaddr_in name;
    int val = 1;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == sock) {
        ret = errno;
        QCAM_ERR("socket() : %s", strerror(errno));
        goto bail;
    }

    if(-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int))) {
        ret = errno;
        QCAM_ERR("setsockopt() : %s", strerror(errno));
        goto bail;
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (-1 == bind(sock, (struct sockaddr *) &name, sizeof (name))) {
        ret = errno;
        QCAM_ERR("bind() : %s", strerror(errno));
        goto bail;
    }

    if (-1 == listen(sock, 1)) {
        ret = errno;
        QCAM_ERR("listen() : %s", strerror(errno));
        goto bail;
    }

bail:
    if (0 != ret) {
        if (-1 != sock) { close(sock); sock = -1; }
    }
    sock_ = sock;
    return ret;
}

int QCamDaemon::run(void)
{
    fd_set active_rfds, rfds;
    int i;
    sigset_t emptyset;
    int maxfd;

    sigemptyset(&emptyset);  /* prepare to unblock all signals in pselect() */
    FD_ZERO (&active_rfds);

    /* add daemon socket to the read descriptors */
    FD_SET(sock_, &active_rfds);
    maxfd = sock_;

    do {
        rfds = active_rfds;

        if (pselect(maxfd + 1, &rfds, NULL, NULL, NULL, &emptyset) < 0) {
            if (EINTR != errno) {
                QCAM_ERR("select : %s", strerror(errno));
            }
            break; /* shutdown server thread */
        }

        // Check server socket first
        if (FD_ISSET(sock_, &rfds)) {
            // Accept new socket on connect
            int new_sock = accept(sock_, NULL, NULL);
            if (new_sock < 0) {
                QCAM_ERR("accept : %s", strerror(errno));
            }
            else {
                QCAM_INFO("Accepted new client connection");
                /* add this socket to read descriptors */
                FD_SET(new_sock, &active_rfds);
                maxfd = (maxfd < new_sock)? new_sock : maxfd;
            }
            FD_CLR(sock_, &rfds);
        }

        // Check all client sockets
        for (i = 0; i < (maxfd + 1) && !stop_; ++i) {
            if (FD_ISSET(i, &rfds)) {
                // Process commands existing connection
                int ret = handleInput(i);
                if (ret < 0) {
                    close(i);
                    FD_CLR(i, &active_rfds);
                }
            }
        } // for
    } while (!stop_);

    QCAM_INFO("QCamDaemon::run() Exit");
    return 0;
}

// Skip spaces (zero or more) at psz.
//
#define IsSpace(c)   ( (c) == ' ' ||  (c) == '\t' || (c) == '\r' || (c) == '\n' )
static __inline void skipSpace(const uint8_t* psz, size_t uMax, size_t& i)
{
   while (i < uMax && IsSpace(psz[i])) {
      ++i;
   }
}

int QCamDaemon::handleInput(int filedes)
{
    int bytes_read;

    bytes_read = read(filedes, &cmd_buf_[cmd_buf_occupied_],
                      CMD_SIZE-cmd_buf_occupied_);
    if (bytes_read < 0) {
        QCAM_ERR("read : %s", strerror(errno));
        return -1;
    }

    if (0 == bytes_read) {  // Socket is closed
        cmd_buf_occupied_ = 0; /* reset the buffer */
        QCAM_INFO("A client connection closed");
        return -2;
    }

    cmd_buf_occupied_ += bytes_read;

    if (cmd_buf_occupied_) {
        size_t bytes_processed;

        cmd_buf_[cmd_buf_occupied_] = '\0';

        current_client_ = filedes;   /* for later retrieval in the message handlers */
        /** check and process the full message */
        handleMessage(cmd_buf_, cmd_buf_occupied_, bytes_processed);

        /** skip over any newlines/spaces after the message */
        skipSpace(cmd_buf_, cmd_buf_occupied_, bytes_processed);

        /** keep the trailing message and append the incoming message to it */
        cmd_buf_occupied_ -= bytes_processed;
        if (cmd_buf_occupied_) {  // move the remaining bytes down
            memmove(cmd_buf_, &cmd_buf_[bytes_processed], cmd_buf_occupied_);
        }
    }

    return 0;
}

int daemonMain(const DaemonConfig& cfg)
{
    QCamDaemon daemon;
    int rc;

    rc = daemon.init(cfg, PORT);

    if (0 == rc) {
        rc = daemon.run();
        daemon.final();
    }

    return rc;
}

}
