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
#ifndef __OMX_VIDEO_ENCODER_CONFIGURE_H__
#define __OMX_VIDEO_ENCODER_CONFIGURE_H__

#include <stdint.h>
#include "OMX_Core.h"
#include "OMX_Video.h"
#include "OMX_QCOMExtns.h"
#include "OMX_IndexExt.h"
#include "OMX_VideoExt.h"

namespace omx {
namespace video {
namespace encoder {

#define OMX_SPEC_VERSION 0x00000101
#define OMX_INIT_STRUCT(_s_, _name_)            \
   memset((_s_), 0x0, sizeof(_name_));          \
   (_s_)->nSize = sizeof(_name_);               \
   (_s_)->nVersion.nVersion = OMX_SPEC_VERSION


// @brief Maximum length for a file name

   static const OMX_S32 VENC_TEST_MAX_STRING = 128;
   static const OMX_U32 PORT_INDEX_IN  = 0;
   static const OMX_U32 PORT_INDEX_OUT = 1;


// @brief Resync marker types

   enum ResyncMarkerType
   {
      RESYNC_MARKER_NONE,  // No resync marker

      RESYNC_MARKER_BITS,  // Resync marker for MPEG4/H.264/H.263
      RESYNC_MARKER_MB,    // MB resync marker for MPEG4/H.264/H.263
      RESYNC_MARKER_GOB    //GOB resync marker for H.263
   };

   enum CodecProfileType
   {
	  MPEG4ProfileSimple,
	  MPEG4ProfileAdvancedSimple,
	  H263ProfileBaseline,
	  AVCProfileBaseline,
	  AVCProfileHigh,
	  AVCProfileMain,
	  VP8ProfileMain,
   };

   enum CodecLevelType
   {
      DefaultLevel,
      VP8Version0,
      VP8Version1,
   };

// @brief Encoder configuration

   struct EncoderConfigType
   {
      ////////////////////////////////////////
      //======== Common static config
      OMX_VIDEO_CODINGTYPE eCodec;                //Config File Key: Codec
      CodecProfileType eCodecProfile;             //Config File Key: Profile
      CodecLevelType eCodecLevel;                 //Config File Key: Level
      OMX_VIDEO_CONTROLRATETYPE eControlRate;     //Config File Key: RC
      OMX_S32 nResyncMarkerSpacing;
      //Config File Key: ResyncMarkerSpacing
      ResyncMarkerType eResyncMarkerType;
      //Config File Key: ResyncMarkerType
      OMX_S32 nIntraRefreshMBCount;
      //Config File Key: IntraRefreshMBCount
      OMX_S32 nFrameWidth;
      //Config File Key: FrameWidth
      OMX_S32 nFrameHeight;
      //Config File Key: FrameHeight
      OMX_S32 nOutputFrameWidth;
      //Config File Key: OutputFrameWidth
      OMX_S32 nOutputFrameHeight;
      //Config File Key: OutputFrameHeight
      OMX_S32 nDVSXOffset;
      //Config File Key: DVSXOffset
      OMX_S32 nDVSYOffset;
      //Config File Key: DVSYOffset
      OMX_S32 nBitrate;                           //Config File Key: Bitrate
      OMX_S32 nFramerate;                         //Config File Key: FPS
      OMX_S32 nRotation;
      //Config File Key: Rotation
      OMX_S32 nInBufferCount;
      //Config File Key: InBufferCount
      OMX_S32 nOutBufferCount;
      //Config File Key: OutBufferCount
      char cInFileName[VENC_TEST_MAX_STRING];     //Config File Key: InFile
      char cOutFileName[VENC_TEST_MAX_STRING];    //Config File Key: OutFile
      OMX_S32 nFrames;
      //Config File Key: NumFrames
      OMX_S32 nIntraPeriod;
      //Config File Key: IntraPeriod
      OMX_S32 nMinQp;                             //Config File Key: MinQp
      OMX_S32 nMaxQp;                             //Config File Key: MaxQp
      OMX_BOOL bProfileMode;
      //Config File Key: ProfileMode
      OMX_BOOL bExtradata;
      //Config file Key: Extradata
      OMX_S32 nIDRPeriod;
      //Config file Key: IDR Period
      OMX_U32 nHierPNumLayers;
      //Config file key: HierPNumLayers
      ////////////////////////////////////////
      //======== Mpeg4 static config
      OMX_S32 nHECInterval;
      //Config File Key: HECInterval
      OMX_S32 nTimeIncRes;
      //Config File Key: TimeIncRes
      OMX_BOOL bEnableShortHeader;
      //Config File Key: EnableShortHeader

      ////////////////////////////////////////
      //======== H.263 static config

      ////////////////////////////////////////
      //======== H.264 static config
      OMX_BOOL bCABAC;                          //Config File Key: CABAC
      OMX_S32 nDeblocker;                       //Config File Key: Deblock
      OMX_U32 id;                               //Config File Key: FramePackId
      OMX_U32 cancel_flag;
      //Config File Key: FramePackCancelFlag
      OMX_U32 type;
      //Config File Key: FramePackType
      OMX_U32 quincunx_sampling_flag;
      //Config File Key: FramePackQuincunxSamplingFlag
      OMX_U32 content_interpretation_type;
      //Config File Key: FramePackContentInterpretationType
      OMX_U32 spatial_flipping_flag;
      //Config File Key: FramePackSpatialFlippingFlag
      OMX_U32 frame0_flipped_flag;
      //Config File Key: FramePackFrame0FlippedFlag
      OMX_U32 field_views_flag;
      //Config File Key: FramePackFieldViewsFlag
      OMX_U32 current_frame_is_frame0_flag;
      //Config File Key: FramePackCurrentFrameIsFrame0Flag
      OMX_U32 frame0_self_contained_flag;
      //Config File Key: FramePackFrame0SelfContainedFlag
      OMX_U32 frame1_self_contained_flag;
      //Config File Key: FramePackFrame1SelfContainedFlag
      OMX_U32 frame0_grid_position_x;
      //Config File Key: FramePackFrame0GridPositionX
      OMX_U32 frame0_grid_position_y;
      //Config File Key: FramePackFrame0GridPositionY
      OMX_U32 frame1_grid_position_x;
      //Config File Key: FramePackFrame1GridPositionX
      OMX_U32 frame1_grid_position_y;
      //Config File Key: FramePackFrame1GridPositionY
      OMX_U32 reserved_byte;
      //Config File Key: FramePackReservedByte
      OMX_U32 repetition_period;
      //Config File Key: FramePackRepetitionPeriod
      OMX_U32 extension_flag;
      //Config File Key: FramePackExtensionFlag
      //======== LTR encoding config
      OMX_S32 nLTRMode;                           //Config File Key: LTR mode
      OMX_S32 nLTRCount;                          //Config File Key: LTR count
      OMX_S32 nLTRPeriod;                         //Config File Key: LTR period

      char cDynamicFile[VENC_TEST_MAX_STRING];
      struct DynamicConfig *pDynamicProperties;
      OMX_U32 dynamic_config_array_size;
   };


// @brief Dynamic configuration
   struct DynamicConfigType
   {
      OMX_S32 nIFrameRequestPeriod;    //Config File Key: IFrameRequestPeriod
      OMX_S32 nUpdatedFramerate;       //Config File Key: UpdatedFPS
      OMX_S32 nUpdatedBitrate;         //Config File Key: UpdatedBitrate
      OMX_S32 nUpdatedFrames;          //Config File Key: UpdatedNumFrames
      OMX_S32 nUpdatedIntraPeriod;     //Config File Key: UpdatedIntraPeriod
   };

   union DynamicConfigData
   {
      OMX_VIDEO_CONFIG_BITRATETYPE bitrate;
      OMX_CONFIG_FRAMERATETYPE framerate;
      QOMX_VIDEO_INTRAPERIODTYPE intraperiod;
      OMX_CONFIG_INTRAREFRESHVOPTYPE intravoprefresh;
      OMX_CONFIG_ROTATIONTYPE rotation;
      OMX_VIDEO_VP8REFERENCEFRAMETYPE vp8refframe;
      OMX_QCOM_VIDEO_CONFIG_LTRMARK_TYPE markltr;
      OMX_QCOM_VIDEO_CONFIG_LTRUSE_TYPE useltr;
      float f_framerate;
   };

   struct DynamicConfig
   {
      unsigned frame_num;
      OMX_INDEXTYPE config_param;
      union DynamicConfigData config_data;
   };

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE Configure(OMX_HANDLETYPE hEncoder, OMX_VIDEO_CODINGTYPE eCodec,
                        OMX_S32& nInputBuffers, OMX_S32& nOutputBuffers,
                        OMX_S32& nInputBufferSize, OMX_S32& nOutputBufferSize,
                        EncoderConfigType& Config);

}}} /* namespace omx::video::encoder */

#endif /* !__OMX_VIDEO_ENCODER_CONFIGURE_H__ */
