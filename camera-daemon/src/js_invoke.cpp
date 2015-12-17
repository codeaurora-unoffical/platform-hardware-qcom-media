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

#include "js_invoke.h"
#include "qcamvid_log.h"
#include "json/json_gen.h"
#include <unistd.h>

void jsCall::dump(void) {
    JSONParser js;
    const char* p;
    int n;

    JSONParser_Ctor(&js, msg_, msg_siz_);

    switch (type_) {
    case Request:
        /* request */
        JSONParser_GetJSON(&js, method_, &p, &n);
        QCAM_INFO("request : %s", std::string(p, n).c_str());
        break;
    case Result:
        /* succesful result */
        JSONParser_GetJSON(&js, result_, &p, &n);
        QCAM_INFO("response : %s", std::string(p, n).c_str());
        break;
    case Error:
        /* failed response */
        JSONParser_GetJSON(&js, error_, &p, &n);
        QCAM_INFO("failed : %s", std::string(p, n).c_str());
        break;
    case Indication:
        /* indication */
        JSONParser_GetJSON(&js, method_, &p, &n);
        QCAM_INFO("indication : %s", std::string(p, n).c_str());
        break;
    default:
        QCAM_ERR("Unsupported message");
    }
}

int jsCall::ctor(const char* msg, int siz)
{
    JSONParser js;
    JSONType jt;
    int rc = 0;

    JSONParser_Ctor(&js, msg, siz);

    if (JSONPARSER_SUCCESS != JSONParser_GetType(&js, 0, &jt)) {
        QCAM_ERR("Invalid JSON type");
        rc = 1;
        goto bail;
    }

    msg_ = msg;
    msg_siz_ = siz;

    (void)JSONParser_Lookup(&js, 0, "method", 0, &method_);
    (void)JSONParser_Lookup(&js, 0, "id", 0, &id_);

    /* our valid messges are JSON objects, the method and id values
       cannot be at 0 offset. */

    /* check for method and id */
    if (0 != id_) {
        if (0 != method_) {
            type_ = Request;
        }
        else {
            if (JSONPARSER_SUCCESS == JSONParser_Lookup(&js, 0, "result", 0,
                                                        &result_)) {
                type_ = Result; /* succesful result */
            }
            else if (JSONPARSER_SUCCESS == JSONParser_Lookup(&js, 0, "error", 0,
                                                             &error_)) {
                type_ = Error; /* failed response */
            }
            else {
                QCAM_ERR("Unsupported message");
                rc = 1;
                goto bail;
            }
        }
    }
    else if (0 != method_) {
        type_ = Indication; /* indication */
    }
    else {
        QCAM_ERR("Unsupported message");
        rc = 1;
        goto bail;
    }

bail:
    return rc;
}

void jsResult_Send(
    int sock,
    unsigned int uid,
    int result /**< 0 is a successful, non zero is error */)
{
    JSONGen gen;
    char buf[128];
    const char *psz;
    int nsize = sizeof(buf);

    JSONGen_Ctor(&gen, buf, nsize, 0, 0);
    JSONGen_BeginObject(&gen);
    JSONGen_PutKey(&gen, "id", 0);
    JSONGen_PutUInt(&gen, uid);

    JSONGen_PutKey(&gen, result == 0 ? "result" : "error", 0);
    JSONGen_PutUInt(&gen, result);

    JSONGen_EndObject(&gen);

    JSONGen_GetJSON(&gen, &psz, &nsize);

    write(sock, psz, nsize);
    QCAM_INFO("RES : %s", psz);
}

