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

#ifndef __QCAMVID_CLIENT_H__
#define __QCAMVID_CLIENT_H__

#include <thread>
#include <atomic>
#include <mutex>
#include "qcamdaemon.h"

namespace camvid {

class Client;
typedef std::shared_ptr<Client> ClientPtr;

/* config, default values are used for initialization. */
struct CmdConfig
{
    bool interactive = false;
    const char* input_file;
};

class Client {
    std::thread th_;
    bool stop_ = false;
    std::mutex lock_;

    uint32_t ti_ = 10; /* timeout in seconds */

    CmdConfig cfg_;
    int sock_ = -1;

    void reader(void);

protected:
    Client() {}
    Client(const Client&) = delete;
    const Client& operator =(const Client&) = delete;

    static ClientPtr make_shared() {
        struct make_shared_enabler : public Client {};
        return std::make_shared<make_shared_enabler>();
    }

public:

    ~Client() { stop(); }

    int start(void);
    void stop(void);

    int send(char* buf, int len);
    void batch_process(void);

    static int create(const CmdConfig& cfg, ClientPtr* pa);
};
}

#endif /* !__QCAMVID_CLIENT_H__ */
