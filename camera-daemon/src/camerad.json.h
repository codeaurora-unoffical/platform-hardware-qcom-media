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

/** JSON string for the default configuration. Can be updated
 *  during runtime. */
const char* default_config = R"****(
{
"cameras" : [
  {
    "id" : 0,
    "name" : "imx214",
    "settings" :
    {
      "fps" : 24,
      "hfr" : 0,
    },
    "sessions" :
    [
      {
        "name" : "720p_fpv",
        "type" : "preview",
        "video_enc" :
        {
          "type" : "h264",
          "bit_rate" : 1000000,
          "width" : 1280,
          "height" : 720,
          "profile" : 1,
          "level" : 1,
        },
      },
      {
        "name" : "4kuhd",
        "type" : "recording",
        "video_enc" :
        {
          "type" : "h264",
          "bit_rate" : 5000000,
          "width" : 3840,
          "height" : 2160,
          "profile" : 1,
          "level" : 1,
        },
        "file_format" : "h264"
      },
    ],
  },
],
}
)****";