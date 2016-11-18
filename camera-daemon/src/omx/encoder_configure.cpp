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
#include "encoder_configure.h"
#include "OMX_Component.h"
#include <sys/syslog.h>
#include <fstream>
#include <string.h>

#define VENC_TEST_MSG_HIGH(fmt, ...)
#define VENC_TEST_MSG_PROFILE(fmt, ...)
#define VENC_TEST_MSG_ERROR(fmt, ...)
#define VENC_TEST_MSG_FATAL(fmt, ...)
#define VENC_TEST_MSG_LOW(fmt, a, b, c)
#define VENC_TEST_MSG_MEDIUM(fmt, a, b, c)

#define Log2(number, power)  { OMX_U32 temp = number; power = 0; while( (0 == (temp & 0x1)) &&  power < 16) { temp >>=0x1; power++; } }
#define FractionToQ16(q,num,den) { OMX_U32 power; Log2(den,power); q = num << (16 - power); }

#define MSM8974

namespace omx {
namespace video {
namespace encoder {

/**
 * @brief Class that wraps/simplifies OMX IL encoder component interactions.
 */
class Encoder {
public:

   /////////////////////////////////////////////////////////////////////////////
   /////////////////////////////////////////////////////////////////////////////
   OMX_ERRORTYPE SetIntraPeriod(OMX_S32 nIntraPeriod)
   {
      OMX_ERRORTYPE result = OMX_ErrorNone;
      QOMX_VIDEO_INTRAPERIODTYPE intra;

      intra.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
      result = OMX_GetConfig(m_hEncoder,
                             (OMX_INDEXTYPE) QOMX_IndexConfigVideoIntraperiod,
                             (OMX_PTR) &intra);

      if (result == OMX_ErrorNone)
      {
         intra.nPFrames = (OMX_U32) nIntraPeriod - 1;
         result = OMX_SetConfig(m_hEncoder,
                                (OMX_INDEXTYPE) QOMX_IndexConfigVideoIntraperiod,
                                (OMX_PTR) &intra);
      }
      else
      {
         VENC_TEST_MSG_ERROR("failed to get state", 0, 0, 0);
      }
      return result;
   }

   /////////////////////////////////////////////////////////////////////////////
   /////////////////////////////////////////////////////////////////////////////
   OMX_ERRORTYPE SetIDRPeriod(OMX_S32 nIntraPeriod, OMX_S32 nIDRPeriod)
   {
      OMX_ERRORTYPE result = OMX_ErrorNone;
      OMX_VIDEO_CONFIG_AVCINTRAPERIOD idr_period;

      idr_period.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
      result = OMX_GetConfig(m_hEncoder,
                             (OMX_INDEXTYPE) OMX_IndexConfigVideoAVCIntraPeriod,
                             (OMX_PTR) &idr_period);
      if (result == OMX_ErrorNone)
      {
         idr_period.nPFrames = (OMX_U32) nIntraPeriod - 1;
         idr_period.nIDRPeriod = (OMX_U32) nIDRPeriod;
         result = OMX_SetConfig(m_hEncoder,
                                (OMX_INDEXTYPE) OMX_IndexConfigVideoAVCIntraPeriod,
                                (OMX_PTR) &idr_period);
      }
      else
      {
         VENC_TEST_MSG_ERROR("failed to get state", 0, 0, 0);
      }
      return result;
   }

   OMX_ERRORTYPE SetHierPNumLayers(OMX_U32 nHierPNumLayers)
   {
     OMX_ERRORTYPE result = OMX_ErrorNone;
     QOMX_VIDEO_HIERARCHICALLAYERS hierP;
     hierP.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
     hierP.nNumLayers = nHierPNumLayers;
     hierP.eHierarchicalCodingType = QOMX_HIERARCHICALCODING_P;
     VENC_TEST_MSG_HIGH("SetParmater nHierPNumLayers = %d", nHierPNumLayers);
     result = OMX_SetParameter(m_hEncoder,
                   (OMX_INDEXTYPE)OMX_QcomIndexHierarchicalStructure,
                   (OMX_PTR) &hierP);
     if(result != OMX_ErrorNone)
     {
         VENC_TEST_MSG_ERROR("Failed to set Hier P Num Layers", 0, 0, 0);
     }
     return result;
   }

   /////////////////////////////////////////////////////////////////////////////
   /////////////////////////////////////////////////////////////////////////////
   /**
    * @brief Configure the encoder
    *
    * @param pConfig The encoder configuration
    */
   OMX_ERRORTYPE Configure(EncoderConfigType* pConfig)
   {
      OMX_ERRORTYPE result = OMX_ErrorNone;
#ifdef _ANDROID_
      char value[PROPERTY_VALUE_MAX] = {0};
#endif
      VENC_TEST_MSG_HIGH("Encoder::Configure");
      if (pConfig == NULL)
      {
         result = OMX_ErrorBadParameter;
      }
      else
      {
         m_nInputBuffers = pConfig->nInBufferCount;
         m_nOutputBuffers = pConfig->nOutBufferCount;
      }
#if 0
fprintf(stderr, "%s : %d\n", "eCodec",                       pConfig->eCodec);
fprintf(stderr, "%s : %d\n", "eCodecProfile",                pConfig->eCodecProfile);
fprintf(stderr, "%s : %d\n", "eCodecLevel",                  pConfig->eCodecLevel);
fprintf(stderr, "%s : %d\n", "eControlRate",                 pConfig->eControlRate);
fprintf(stderr, "%s : %d\n", "nResyncMarkerSpacing",         pConfig->nResyncMarkerSpacing);
fprintf(stderr, "%s : %d\n", "eResyncMarkerType",            pConfig->eResyncMarkerType);
fprintf(stderr, "%s : %d\n", "nIntraRefreshMBCount",         pConfig->nIntraRefreshMBCount);
fprintf(stderr, "%s : %d\n", "nFrameWidth",                  pConfig->nFrameWidth);
fprintf(stderr, "%s : %d\n", "nFrameHeight",                 pConfig->nFrameHeight);
fprintf(stderr, "%s : %d\n", "nOutputFrameWidth",            pConfig->nOutputFrameWidth);
fprintf(stderr, "%s : %d\n", "nOutputFrameHeight",           pConfig->nOutputFrameHeight);
fprintf(stderr, "%s : %d\n", "nDVSXOffset",                  pConfig->nDVSXOffset);
fprintf(stderr, "%s : %d\n", "nDVSYOffset",                  pConfig->nDVSYOffset);
fprintf(stderr, "%s : %d\n", "nBitrate",                     pConfig->nBitrate);
fprintf(stderr, "%s : %d\n", "nFramerate",                   pConfig->nFramerate);
fprintf(stderr, "%s : %d\n", "nRotation",                    pConfig->nRotation);
fprintf(stderr, "%s : %d\n", "nInBufferCount",               pConfig->nInBufferCount);
fprintf(stderr, "%s : %d\n", "nOutBufferCount",              pConfig->nOutBufferCount);
fprintf(stderr, "%s : %d\n", "cInFileName",                  pConfig->cInFileName);
fprintf(stderr, "%s : %d\n", "cOutFileName",                 pConfig->cOutFileName);
fprintf(stderr, "%s : %d\n", "nFrames",                      pConfig->nFrames);
fprintf(stderr, "%s : %d\n", "nIntraPeriod",                 pConfig->nIntraPeriod);
fprintf(stderr, "%s : %d\n", "nMinQp",                       pConfig->nMinQp);
fprintf(stderr, "%s : %d\n", "nMaxQp",                       pConfig->nMaxQp);
fprintf(stderr, "%s : %d\n", "bProfileMode",                 pConfig->bProfileMode);
fprintf(stderr, "%s : %d\n", "bExtradata",                   pConfig->bExtradata);
fprintf(stderr, "%s : %d\n", "nIDRPeriod",                   pConfig->nIDRPeriod);
fprintf(stderr, "%s : %d\n", "nHierPNumLayers",              pConfig->nHierPNumLayers);
fprintf(stderr, "%s : %d\n", "nHECInterval",                 pConfig->nHECInterval);
fprintf(stderr, "%s : %d\n", "nTimeIncRes",                  pConfig->nTimeIncRes);
fprintf(stderr, "%s : %d\n", "bEnableShortHeader",           pConfig->bEnableShortHeader);
fprintf(stderr, "%s : %d\n", "bCABAC",                       pConfig->bCABAC);
fprintf(stderr, "%s : %d\n", "nDeblocker",                   pConfig->nDeblocker);
fprintf(stderr, "%s : %d\n", "id",                           pConfig->id);
fprintf(stderr, "%s : %d\n", "cancel_flag",                  pConfig->cancel_flag);
fprintf(stderr, "%s : %d\n", "type",                         pConfig->type);
fprintf(stderr, "%s : %d\n", "quincunx_sampling_flag",       pConfig->quincunx_sampling_flag);
fprintf(stderr, "%s : %d\n", "content_interpretation_type",  pConfig->content_interpretation_type);
fprintf(stderr, "%s : %d\n", "spatial_flipping_flag",        pConfig->spatial_flipping_flag);
fprintf(stderr, "%s : %d\n", "frame0_flipped_flag",          pConfig->frame0_flipped_flag);
fprintf(stderr, "%s : %d\n", "field_views_flag",             pConfig->field_views_flag);
fprintf(stderr, "%s : %d\n", "current_frame_is_frame0_flag", pConfig->current_frame_is_frame0_flag);
fprintf(stderr, "%s : %d\n", "frame0_self_contained_flag",   pConfig->frame0_self_contained_flag);
fprintf(stderr, "%s : %d\n", "frame1_self_contained_flag",   pConfig->frame1_self_contained_flag);
fprintf(stderr, "%s : %d\n", "frame0_grid_position_x",       pConfig->frame0_grid_position_x);
fprintf(stderr, "%s : %d\n", "frame0_grid_position_y",       pConfig->frame0_grid_position_y);
fprintf(stderr, "%s : %d\n", "frame1_grid_position_x",       pConfig->frame1_grid_position_x);
fprintf(stderr, "%s : %d\n", "frame1_grid_position_y",       pConfig->frame1_grid_position_y);
fprintf(stderr, "%s : %d\n", "reserved_byte",                pConfig->reserved_byte);
fprintf(stderr, "%s : %d\n", "repetition_period",            pConfig->repetition_period);
fprintf(stderr, "%s : %d\n", "extension_flag",               pConfig->extension_flag);
fprintf(stderr, "%s : %d\n", "nLTRMode",                     pConfig->nLTRMode);
fprintf(stderr, "%s : %d\n", "nLTRCount",                    pConfig->nLTRCount);
fprintf(stderr, "%s : %d\n", "nLTRPeriod",                   pConfig->nLTRPeriod);
#endif
      //////////////////////////////////////////
      // codec specific config
      //////////////////////////////////////////
      if (result == OMX_ErrorNone)
      {
         if (m_eCodec == OMX_VIDEO_CodingMPEG4)
         {
            OMX_VIDEO_PARAM_MPEG4TYPE mp4;
            mp4.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
            result = OMX_GetParameter(m_hEncoder,
                                      OMX_IndexParamVideoMpeg4,
                                      (OMX_PTR) &mp4);

            if (result == OMX_ErrorNone)
            {
               mp4.nTimeIncRes = pConfig->nTimeIncRes;
               mp4.nPFrames = pConfig->nIntraPeriod - 1;
               if((pConfig->eCodecProfile == MPEG4ProfileSimple) ||
                  (pConfig->eCodecProfile == MPEG4ProfileAdvancedSimple))
               {
                  switch(pConfig->eCodecProfile)
                  {
                  case MPEG4ProfileSimple:
                     mp4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
                     mp4.nBFrames = 0;
                     break;
                  case MPEG4ProfileAdvancedSimple:
                     mp4.eProfile = OMX_VIDEO_MPEG4ProfileAdvancedSimple;
                     mp4.nBFrames = 1;
                     mp4.nPFrames = pConfig->nIntraPeriod/2;
                     break;
                  default:
                     mp4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
                     mp4.nBFrames = 0;
                     break;
                  }
               }
               else
               {
                  VENC_TEST_MSG_MEDIUM("Invalid MPEG4 Profile set defaulting to Simple Profile",0,0,0);
                  mp4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
               }
               mp4.eLevel = OMX_VIDEO_MPEG4Level0;
               mp4.nSliceHeaderSpacing = 0;
               mp4.bReversibleVLC = OMX_FALSE;
               mp4.bACPred = OMX_TRUE;
               mp4.bGov = OMX_FALSE;
               mp4.bSVH = pConfig->bEnableShortHeader;
               mp4.nHeaderExtension = 0;

               result = OMX_SetParameter(m_hEncoder,
                                         OMX_IndexParamVideoMpeg4,
                                         (OMX_PTR) &mp4);
            }
         }
         else if (m_eCodec == OMX_VIDEO_CodingH263)
         {
            OMX_VIDEO_PARAM_H263TYPE h263;
            h263.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
            result = OMX_GetParameter(m_hEncoder,
                                      OMX_IndexParamVideoH263,
                                      (OMX_PTR) &h263);

            if (result == OMX_ErrorNone)
            {
               h263.nPFrames = pConfig->nIntraPeriod - 1;
               h263.nBFrames = 0;
               if(pConfig->eCodecProfile == H263ProfileBaseline)
               {
                  h263.eProfile = OMX_VIDEO_H263ProfileBaseline;
               }
               else
               {
                  VENC_TEST_MSG_MEDIUM("Invalid H263 Profile set defaulting to Baseline Profile",0,0,0);
                  h263.eProfile = (OMX_VIDEO_H263PROFILETYPE) OMX_VIDEO_H263ProfileBaseline;
               }
               h263.eLevel = OMX_VIDEO_H263Level10;
               h263.bPLUSPTYPEAllowed = OMX_FALSE;
               h263.nAllowedPictureTypes = 2;
               h263.bForceRoundingTypeToZero = OMX_TRUE;
               h263.nPictureHeaderRepetition = 0;
               h263.nGOBHeaderInterval = 1;

               result = OMX_SetParameter(m_hEncoder,
                                         OMX_IndexParamVideoH263,
                                         (OMX_PTR) &h263);
            }
         }
         else if (m_eCodec == OMX_VIDEO_CodingAVC)
         {
            OMX_VIDEO_PARAM_AVCTYPE avc;
            avc.nSize = sizeof(avc);
            avc.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
            result = OMX_GetParameter(m_hEncoder,
                                      OMX_IndexParamVideoAvc,
                                      (OMX_PTR) &avc);

            if (result == OMX_ErrorNone)
            {
               avc.nPFrames = pConfig->nIntraPeriod - 1;
               avc.nBFrames = 0;
               if((pConfig->eCodecProfile == AVCProfileBaseline) ||
                  (pConfig->eCodecProfile == AVCProfileMain)||
                  (pConfig->eCodecProfile == AVCProfileHigh))
               {
                  switch(pConfig->eCodecProfile)
                  {
                  case AVCProfileBaseline:
                     avc.eProfile = OMX_VIDEO_AVCProfileBaseline;
                     avc.nBFrames = 0;
                     break;
                  case AVCProfileHigh:
                     avc.eProfile = OMX_VIDEO_AVCProfileHigh;
                     avc.nBFrames = 1;
                     avc.nPFrames = pConfig->nIntraPeriod/2;
                     break;
                  case AVCProfileMain:
                     avc.eProfile = OMX_VIDEO_AVCProfileMain;
                     avc.nBFrames = 1;
                     avc.nPFrames = pConfig->nIntraPeriod/2;
                     break;
                  default:
                     avc.eProfile = OMX_VIDEO_AVCProfileBaseline;
                     avc.nBFrames = 0;
                     break;
                  }
#ifdef _ANDROID_
                  OMX_U32 slice_delivery_mode = 0;
                  property_get("vidc.venc.slicedeliverymode", value, "0");
                  slice_delivery_mode = atoi(value);
                  if (slice_delivery_mode) {
                     avc.nBFrames = 0;
                  }
#endif
               }
               else
               {
                  VENC_TEST_MSG_ERROR("Invalid H264 Profile set defaulting to Baseline Profile",0,0,0);
                  avc.eProfile = OMX_VIDEO_AVCProfileBaseline;
               }

               avc.eLevel = OMX_VIDEO_AVCLevel1;
               avc.bUseHadamard = OMX_FALSE;
               avc.nRefFrames = 1;
               avc.nRefIdx10ActiveMinus1 = 1;
               avc.nRefIdx11ActiveMinus1 = 0;
               avc.bEnableUEP = OMX_FALSE;
               avc.bEnableFMO = OMX_FALSE;
               avc.bEnableASO = OMX_FALSE;
               avc.bEnableRS = OMX_FALSE;
               avc.nAllowedPictureTypes = 2;
               avc.bFrameMBsOnly = OMX_FALSE;
               avc.bMBAFF = OMX_FALSE;
               avc.bWeightedPPrediction = OMX_FALSE;
               avc.nWeightedBipredicitonMode = 0;
               avc.bconstIpred = OMX_FALSE;
               avc.bDirect8x8Inference = OMX_FALSE;
               avc.bDirectSpatialTemporal = OMX_FALSE;
               if(avc.eProfile == OMX_VIDEO_AVCProfileBaseline)
               {
                  avc.bEntropyCodingCABAC = OMX_FALSE;
                  avc.nCabacInitIdc = 0;
               }
               else
               {
                  if (pConfig->bCABAC)
                  {
                     avc.bEntropyCodingCABAC = OMX_TRUE;
                     avc.nCabacInitIdc = 0;
                  }
                  else
                  {
                     avc.bEntropyCodingCABAC = OMX_FALSE;
                     avc.nCabacInitIdc = 0;
                  }
               }

               switch(pConfig->nDeblocker)
               {
               case 0:
                  avc.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterDisable;
                  break;
               case 1:
                  avc.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;
                  break;
               case 2:
                  avc.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterDisableSliceBoundary;
                  break;
               default:
                  avc.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;
                  break;
               }
               result = OMX_SetParameter(m_hEncoder,
                                         OMX_IndexParamVideoAvc,
                                         (OMX_PTR) &avc);
            }
#ifdef MSM8974
            if (pConfig->nIDRPeriod)
               result = SetIDRPeriod(pConfig->nIntraPeriod - 1, pConfig->nIDRPeriod);
#endif
#ifndef MSM8974
            ////////////////////////////////////////
            // SEI FramePack data
            ////////////////////////////////////////
            if (result == OMX_ErrorNone)
            {

               OMX_QCOM_FRAME_PACK_ARRANGEMENT framePackingArrangement;
               memset(&framePackingArrangement, 0, sizeof(framePackingArrangement));
               framePackingArrangement.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
               framePackingArrangement.id = pConfig->id;
               framePackingArrangement.cancel_flag = pConfig->cancel_flag;
               framePackingArrangement.type = pConfig->type;
               framePackingArrangement.quincunx_sampling_flag = pConfig->quincunx_sampling_flag;
               framePackingArrangement.content_interpretation_type = pConfig->content_interpretation_type;
               framePackingArrangement.spatial_flipping_flag = pConfig->spatial_flipping_flag;
               framePackingArrangement.frame0_flipped_flag = pConfig->frame0_flipped_flag;
               framePackingArrangement.field_views_flag = pConfig->field_views_flag;
               framePackingArrangement.current_frame_is_frame0_flag = pConfig->current_frame_is_frame0_flag;
               framePackingArrangement.frame0_self_contained_flag = pConfig->frame0_self_contained_flag;
               framePackingArrangement.frame1_self_contained_flag = pConfig->frame1_self_contained_flag;
               framePackingArrangement.frame0_grid_position_x = pConfig->frame0_grid_position_x;
               framePackingArrangement.frame0_grid_position_y = pConfig->frame0_grid_position_y;
               framePackingArrangement.frame1_grid_position_x = pConfig->frame1_grid_position_x;
               framePackingArrangement.frame1_grid_position_y = pConfig->frame1_grid_position_y;
               framePackingArrangement.reserved_byte = pConfig->reserved_byte;
               framePackingArrangement.repetition_period = pConfig->repetition_period;
               framePackingArrangement.extension_flag = pConfig->extension_flag;

               result = OMX_SetConfig(m_hEncoder,
                                      (OMX_INDEXTYPE)OMX_QcomIndexConfigVideoFramePackingArrangement,
                                      (OMX_PTR) &framePackingArrangement);

            }
#endif
         }
      }

      //////////////////////////////////////////
      // error resilience
      //////////////////////////////////////////
	   OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE errorCorrection; //OMX_IndexParamVideoErrorCorrection
		errorCorrection.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
        errorCorrection.nSize = sizeof(errorCorrection);
		result = OMX_GetParameter(m_hEncoder,
								(OMX_INDEXTYPE) OMX_IndexParamVideoErrorCorrection,
								(OMX_PTR) &errorCorrection);

		errorCorrection.bEnableRVLC = OMX_FALSE;
		errorCorrection.bEnableDataPartitioning = OMX_FALSE;

		if (result == OMX_ErrorNone)
		{
         if (pConfig->eResyncMarkerType != RESYNC_MARKER_NONE)
         {
            if(pConfig->eResyncMarkerType != RESYNC_MARKER_MB)
            {
               if ((pConfig->eResyncMarkerType == RESYNC_MARKER_BITS) &&
                  (m_eCodec == OMX_VIDEO_CodingMPEG4))
               {
                  errorCorrection.bEnableResync = OMX_TRUE;
                  errorCorrection.nResynchMarkerSpacing = pConfig->nResyncMarkerSpacing;
                  errorCorrection.bEnableHEC = pConfig->nHECInterval == 0 ? OMX_FALSE : OMX_TRUE;
               }
               else if ((pConfig->eResyncMarkerType == RESYNC_MARKER_BITS) &&
                      (m_eCodec == OMX_VIDEO_CodingAVC))
               {
                  errorCorrection.bEnableResync = OMX_TRUE;
                  errorCorrection.nResynchMarkerSpacing = pConfig->nResyncMarkerSpacing;
               }
               else if ((pConfig->eResyncMarkerType == RESYNC_MARKER_GOB) &&
                      (m_eCodec == OMX_VIDEO_CodingH263))
               {
                  errorCorrection.bEnableResync = OMX_FALSE;
                  errorCorrection.nResynchMarkerSpacing = pConfig->nResyncMarkerSpacing;
                  errorCorrection.bEnableDataPartitioning = OMX_TRUE;
               }

               result = OMX_SetParameter(m_hEncoder,
                                   (OMX_INDEXTYPE) OMX_IndexParamVideoErrorCorrection,
                                   (OMX_PTR) &errorCorrection);
            }
            else
            {
               if (m_eCodec == OMX_VIDEO_CodingAVC)
               {
                  OMX_VIDEO_PARAM_AVCTYPE avcdata;
                  avcdata.nSize = sizeof(avcdata);
                  avcdata.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
                  result = OMX_GetParameter(m_hEncoder,
                                            OMX_IndexParamVideoAvc,
                                            (OMX_PTR) &avcdata);
                  if (result == OMX_ErrorNone)
                  {
                     VENC_TEST_MSG_HIGH("slice mode enabled with slice size = %d",
                         pConfig->nResyncMarkerSpacing);
                     avcdata.nSliceHeaderSpacing = pConfig->nResyncMarkerSpacing;
                     result = OMX_SetParameter(m_hEncoder,
                                               OMX_IndexParamVideoAvc,
                                               (OMX_PTR) &avcdata);
                  }
                  OMX_U32 slice_delivery_mode = 0;
#ifdef _ANDROID_
                  property_get("vidc.venc.slicedeliverymode", value, "0");
                  slice_delivery_mode = atoi(value);
#endif
                  if ((result == OMX_ErrorNone) && (slice_delivery_mode))
                  {
                     VENC_TEST_MSG_HIGH("Slice delivery mode enabled via setprop command", 0, 0, 0);
                     QOMX_EXTNINDEX_PARAMTYPE extnIndex;
                     extnIndex.nPortIndex = PORT_INDEX_OUT;
                     extnIndex.bEnable = OMX_TRUE;
                     result = OMX_SetParameter(m_hEncoder,
                                               (OMX_INDEXTYPE)OMX_QcomIndexEnableSliceDeliveryMode,
                                               (OMX_PTR) &extnIndex);
                     if (result != OMX_ErrorNone)
                        VENC_TEST_MSG_ERROR("Failed to set slice delivery mode", 0, 0, 0);

                     // update input port definition
                     OMX_PARAM_PORTDEFINITIONTYPE inPortDef;
                     inPortDef.nSize = sizeof(inPortDef);
                     inPortDef.nPortIndex = (OMX_U32) PORT_INDEX_IN;
                     result = OMX_GetParameter(m_hEncoder,
                                               OMX_IndexParamPortDefinition,
                                               (OMX_PTR) &inPortDef);
                     if (result != OMX_ErrorNone)
                        VENC_TEST_MSG_ERROR("Failed to get input port definition", 0, 0, 0);
                     m_nInputBufferSize = (OMX_S32) inPortDef.nBufferSize;
                     if(m_nInputBuffers < (OMX_S32)inPortDef.nBufferCountMin)
                     {
                        m_nInputBuffers = (OMX_S32) inPortDef.nBufferCountMin;
                        inPortDef.nBufferCountActual = inPortDef.nBufferCountMin;
                        pConfig->nInBufferCount = m_nInputBuffers;
                     }
                     VENC_TEST_MSG_HIGH("Set updated inbuf: m_nInputBufferSize = %d min count = %d "
                         "actual = %d", m_nInputBufferSize, inPortDef.nBufferCountMin,
                         inPortDef.nBufferCountActual);
                     result = OMX_SetParameter(m_hEncoder,
                                            OMX_IndexParamPortDefinition,
                                            (OMX_PTR) &inPortDef);
                     if (result != OMX_ErrorNone)
                        VENC_TEST_MSG_ERROR("Failed to set input port definition", 0, 0, 0);

                     // update output port definition
                     OMX_PARAM_PORTDEFINITIONTYPE outPortDef;
                     outPortDef.nSize = sizeof(outPortDef);
                     outPortDef.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
                     result = OMX_GetParameter(m_hEncoder,
                                               OMX_IndexParamPortDefinition,
                                               (OMX_PTR) &outPortDef);
                     if (result != OMX_ErrorNone)
                        VENC_TEST_MSG_ERROR("Failed to get output port definition", 0, 0, 0);
                     m_nOutputBufferSize = (OMX_S32) outPortDef.nBufferSize;
                     if(m_nOutputBuffers < (OMX_S32)outPortDef.nBufferCountMin)
                     {
                        m_nOutputBuffers = (OMX_S32) outPortDef.nBufferCountMin;
                        outPortDef.nBufferCountActual = outPortDef.nBufferCountMin;
                        pConfig->nOutBufferCount = m_nOutputBuffers;
                     }
                     VENC_TEST_MSG_HIGH("Set updated outbuf: m_nOutputBufferSize = %d min count = %d "
                         "actual = %d", m_nOutputBufferSize, outPortDef.nBufferCountMin,
                         outPortDef.nBufferCountActual);
                     result = OMX_SetParameter(m_hEncoder,
                                            OMX_IndexParamPortDefinition,
                                            (OMX_PTR) &outPortDef);
                     if (result != OMX_ErrorNone)
                        VENC_TEST_MSG_ERROR("Failed to set output port definition", 0, 0, 0);
                  }
               }
               else if (m_eCodec == OMX_VIDEO_CodingMPEG4)
               {
                  OMX_VIDEO_PARAM_MPEG4TYPE mp4;
                  mp4.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
                  result = OMX_GetParameter(m_hEncoder,
                                            OMX_IndexParamVideoMpeg4,
                                            (OMX_PTR) &mp4);
                  if (result == OMX_ErrorNone)
                  {
                     mp4.nSliceHeaderSpacing = pConfig->nResyncMarkerSpacing;
                     result = OMX_SetParameter(m_hEncoder,
                                               OMX_IndexParamVideoMpeg4,
                                               (OMX_PTR) &mp4);
                  }
               }
            }
         }
      }
#ifndef MAX_RES_720P

      //////////////////////////////////////////
      // Slice info extradata
      // Note: ExtraData should be set before OMX_IndexParamPortDefinition
      //       else driver will through error allocating input/output buffers
      //////////////////////////////////////////
      if (pConfig->bExtradata && result == OMX_ErrorNone)
      {
         QOMX_INDEXEXTRADATATYPE extraData; // OMX_QcomIndexParamIndexExtraDataType
         extraData.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
         extraData.nIndex = (OMX_INDEXTYPE)OMX_ExtraDataVideoEncoderSliceInfo;
         VENC_TEST_MSG_HIGH("GetParameter OMX_QcomIndexParamIndexExtraDataType");
         result = OMX_GetParameter(m_hEncoder,
                     (OMX_INDEXTYPE)OMX_QcomIndexParamIndexExtraDataType,
                     (OMX_PTR) &extraData);
         if (result == OMX_ErrorNone)
         {
            VENC_TEST_MSG_HIGH("GetParameter ExtraDataSliceInfo = %d",
                    extraData.bEnabled);
            if (extraData.bEnabled == OMX_FALSE)
            {
               extraData.bEnabled = OMX_TRUE;
               VENC_TEST_MSG_HIGH("SetParameter ExtraDataSliceInfo = %d",
                        extraData.bEnabled);
               result = OMX_SetParameter(m_hEncoder,
                            (OMX_INDEXTYPE)OMX_QcomIndexParamIndexExtraDataType,
                            (OMX_PTR) &extraData);
               if (result != OMX_ErrorNone)
               {
                  VENC_TEST_MSG_ERROR("Setting Slice information "
                            "extradata failed ...", 0, 0, 0);
               }
            }
         }
      }

      /* LTR encoding settings */
      if ((result == OMX_ErrorNone) && (pConfig->nLTRMode > 0))
      {
#ifndef MSM8974
         QOMX_INDEXEXTRADATATYPE extraData;
         extraData.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
         extraData.nIndex = (OMX_INDEXTYPE)OMX_ExtraDataVideoLTRInfo;
         VENC_TEST_MSG_HIGH("GetParameter OMX_QcomIndexParamIndexExtraDataType");
         result = OMX_GetParameter(m_hEncoder,
                    (OMX_INDEXTYPE)OMX_QcomIndexParamIndexExtraDataType,
                    (OMX_PTR) &extraData);
         if (result == OMX_ErrorNone)
         {
            VENC_TEST_MSG_HIGH("GetParameter ExtraDataVideoLTRInfo = %d",
                    extraData.bEnabled);
            if (extraData.bEnabled == OMX_FALSE)
            {
               extraData.bEnabled = OMX_TRUE;
               VENC_TEST_MSG_HIGH("SetParameter ExtraDataVideoLTRInfo = %d",
                        extraData.bEnabled);
               result = OMX_SetParameter(m_hEncoder,
                            (OMX_INDEXTYPE)OMX_QcomIndexParamIndexExtraDataType,
                            (OMX_PTR) &extraData);
               if (result != OMX_ErrorNone)
               {
                  VENC_TEST_MSG_ERROR("Setting LTR information "
                        "extradata failed ...", 0, 0, 0);
               }
            }
         }
         /* get capability */
         QOMX_EXTNINDEX_RANGETYPE  ltrCountRange;
         OMX_INIT_STRUCT(&ltrCountRange, QOMX_EXTNINDEX_RANGETYPE);
         ltrCountRange.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
         result = OMX_GetParameter(m_hEncoder,
                        (OMX_INDEXTYPE)QOMX_IndexParamVideoLTRCountRangeSupported,
                        (OMX_PTR) &ltrCountRange);
         if(OMX_ErrorNone == result)
         {
            if (pConfig->nLTRCount > ltrCountRange.nMax)
            {
               VENC_TEST_MSG_ERROR("Test app configuring LTR count %d larger than "
                   "supported %d...", pConfig->nLTRCount, ltrCountRange.nMax, 0);
               result = OMX_ErrorInsufficientResources;
            }
            else
            {
               VENC_TEST_MSG_HIGH("Encoder LTR coding capability: Max %d, Min %d, "
                   "Stepsize %d", ltrCountRange.nMax, ltrCountRange.nMin,
                   ltrCountRange.nStepSize);
            }
         }
         else
         {
            VENC_TEST_MSG_ERROR("Get capability on LTR Count failed 0x%x ...",
                result, 0, 0);
         }
#endif

         /* Configure LTR mode */
         if (OMX_ErrorNone == result)
         {
            QOMX_VIDEO_PARAM_LTRMODE_TYPE  ltrMode;
            OMX_INIT_STRUCT(&ltrMode, QOMX_VIDEO_PARAM_LTRMODE_TYPE);
            ltrMode.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
            if(pConfig->nLTRMode == 0)
            {
               ltrMode.eLTRMode = QOMX_VIDEO_LTRMode_Disable;
            }
            else if  (pConfig->nLTRMode == 1)
            {
               ltrMode.eLTRMode = QOMX_VIDEO_LTRMode_Manual;
            }
            else if(pConfig->nLTRMode == 2)
            {
               ltrMode.eLTRMode = QOMX_VIDEO_LTRMode_Auto;
            }

            result = OMX_SetParameter(m_hEncoder,
                            (OMX_INDEXTYPE)QOMX_IndexParamVideoLTRMode,
                            (OMX_PTR) &ltrMode);
            if (OMX_ErrorNone != result)
            {
               VENC_TEST_MSG_ERROR("Set LTR Mode failed with status %d ...",
                   result, 0, 0);
            }
         }

         /* Configure LTR count */
         if (OMX_ErrorNone == result)
         {
            QOMX_VIDEO_PARAM_LTRCOUNT_TYPE  ltrCount;
            OMX_INIT_STRUCT(&ltrCount, QOMX_VIDEO_PARAM_LTRCOUNT_TYPE);
            ltrCount.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
            ltrCount.nCount = pConfig->nLTRCount;

            result = OMX_SetParameter(m_hEncoder,
                            (OMX_INDEXTYPE)QOMX_IndexParamVideoLTRCount,
                            (OMX_PTR) &ltrCount);
            if (OMX_ErrorNone != result)
            {
               VENC_TEST_MSG_ERROR("Set LTR Count failed with status %d ...",
                   result, 0, 0);
            }
         }

         /* Configure LTR period */
         if (OMX_ErrorNone == result)
         {
            QOMX_VIDEO_CONFIG_LTRPERIOD_TYPE  ltrPeriod;
            OMX_INIT_STRUCT(&ltrPeriod, QOMX_VIDEO_CONFIG_LTRPERIOD_TYPE);
            ltrPeriod.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
            ltrPeriod.nFrames = pConfig->nLTRPeriod - 1;

            result = OMX_SetConfig(m_hEncoder,
                            (OMX_INDEXTYPE)QOMX_IndexConfigVideoLTRPeriod,
                            (OMX_PTR) &ltrPeriod);
            if (OMX_ErrorNone != result)
            {
               VENC_TEST_MSG_ERROR("Set LTR Period failed with status %d ...",
                   result, 0, 0);
            }
         }
      }
#endif

      //////////////////////////////////////////
      // input buffer requirements
      //////////////////////////////////////////
      if (result == OMX_ErrorNone)
      {
         OMX_PARAM_PORTDEFINITIONTYPE inPortDef; // OMX_IndexParamPortDefinition
         inPortDef.nSize = sizeof(inPortDef);
         inPortDef.nPortIndex = (OMX_U32) PORT_INDEX_IN; // input
         result = OMX_GetParameter(m_hEncoder,
                                   OMX_IndexParamPortDefinition,
                                   (OMX_PTR) &inPortDef);
         if (result == OMX_ErrorNone)
         {
            inPortDef.format.video.nFrameWidth = pConfig->nFrameWidth;
            inPortDef.format.video.nFrameHeight = pConfig->nFrameHeight;
            result = OMX_SetParameter(m_hEncoder,
                                      OMX_IndexParamPortDefinition,
                                      (OMX_PTR) &inPortDef);
            result = OMX_GetParameter(m_hEncoder,
                                      OMX_IndexParamPortDefinition,
                                      (OMX_PTR) &inPortDef);
            if(m_nInputBuffers > (OMX_S32)inPortDef.nBufferCountMin)
            {
#ifdef MSM8974
               inPortDef.nBufferCountActual = inPortDef.nBufferCountMin = m_nInputBuffers;
#else
               inPortDef.nBufferCountActual = m_nInputBuffers;
#endif
            }
            else
            {
              VENC_TEST_MSG_ERROR("IN Buffer min cnt mismatch ...%d != %d\n",
                                  inPortDef.nBufferCountMin, m_nInputBuffers, 0);
            }
            result = OMX_SetParameter(m_hEncoder,
                                      OMX_IndexParamPortDefinition,
                                      (OMX_PTR) &inPortDef);

            result = OMX_GetParameter(m_hEncoder,
                                      OMX_IndexParamPortDefinition,
                                      (OMX_PTR) &inPortDef);

            if (m_nInputBuffers != (OMX_S32) inPortDef.nBufferCountActual)
            {
               VENC_TEST_MSG_ERROR("IN: Buffer reqs dont match...%d != %d\n",
                                   inPortDef.nBufferCountActual, m_nInputBuffers, 0);
            }
         }
         m_nInputBufferSize = (OMX_S32) inPortDef.nBufferSize;
      }

      //////////////////////////////////////////
      // output buffer requirements
      //////////////////////////////////////////
      if (result == OMX_ErrorNone)
      {
         OMX_PARAM_PORTDEFINITIONTYPE outPortDef; // OMX_IndexParamPortDefinition
         outPortDef.nSize = sizeof(outPortDef);
         outPortDef.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
         result = OMX_GetParameter(m_hEncoder,
                                   OMX_IndexParamPortDefinition,
                                   (OMX_PTR) &outPortDef);
         if (result == OMX_ErrorNone)
         {
            outPortDef.format.video.nFrameWidth = pConfig->nOutputFrameWidth;
            outPortDef.format.video.nFrameHeight = pConfig->nOutputFrameHeight;
            FractionToQ16(outPortDef.format.video.xFramerate,
                (int)(pConfig->nFramerate * 2),2);
            result = OMX_SetParameter(m_hEncoder,
                                      OMX_IndexParamPortDefinition,
                                      (OMX_PTR) &outPortDef);
            result = OMX_GetParameter(m_hEncoder,
                                      OMX_IndexParamPortDefinition,
                                      (OMX_PTR) &outPortDef);
            if(m_nOutputBuffers > (OMX_S32)outPortDef.nBufferCountMin)
            {
              outPortDef.nBufferCountActual = m_nOutputBuffers;
            }
            else
            {
              VENC_TEST_MSG_ERROR("OUT: Buffer min cnt mismatch ...%d != %d\n",
                                  outPortDef.nBufferCountActual, m_nOutputBuffers, 0);
              m_nOutputBuffers = (OMX_S32) outPortDef.nBufferCountMin;
              pConfig->nOutBufferCount = m_nOutputBuffers;

            }

            result = OMX_SetParameter(m_hEncoder,
                                      OMX_IndexParamPortDefinition,
                                      (OMX_PTR) &outPortDef);
            result = OMX_GetParameter(m_hEncoder,
                                      OMX_IndexParamPortDefinition,
                                      (OMX_PTR) &outPortDef);
            if (m_nOutputBuffers != (OMX_S32) outPortDef.nBufferCountActual)
            {
               VENC_TEST_MSG_ERROR("OUT: Buffer reqs dont match...%d != %d\n",
                                   (OMX_S32)outPortDef.nBufferCountActual, m_nOutputBuffers, 0);
               m_nOutputBuffers = (OMX_S32) outPortDef.nBufferCountMin;
               pConfig->nOutBufferCount = m_nOutputBuffers;

            }
         }
         m_nOutputBufferSize = (OMX_S32) outPortDef.nBufferSize;
      }

      // bitrate
      //////////////////////////////////////////
      if (result == OMX_ErrorNone)
      {
         OMX_VIDEO_PARAM_BITRATETYPE bitrate; // OMX_IndexParamVideoBitrate
         bitrate.nSize = sizeof(bitrate);
         bitrate.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
         result = OMX_GetParameter(m_hEncoder,
                                   OMX_IndexParamVideoBitrate,
                                   (OMX_PTR) &bitrate);
         if (result == OMX_ErrorNone)
         {
            bitrate.eControlRate = pConfig->eControlRate;
            bitrate.nTargetBitrate = pConfig->nBitrate;
            result = OMX_SetParameter(m_hEncoder,
                                      OMX_IndexParamVideoBitrate,
                                      (OMX_PTR) &bitrate);
         }
      }
#ifndef MSM8974
#ifdef _ANDROID_
      //////////////////////////////////////////
      // maximum allowed bitrate check
      //////////////////////////////////////////
      OMX_U32 bitratecheck = 0;
      property_get("vidc.venc.debug.bitratecheck", value, "0");
      bitratecheck = atoi(value);
      if ((result == OMX_ErrorNone) && (bitratecheck))
      {
         VENC_TEST_MSG_HIGH("bitratecheck is enabled via setprop command");
         QOMX_EXTNINDEX_PARAMTYPE bitrate_check;
         bitrate_check.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
         bitrate_check.bEnable = OMX_TRUE;
         result = OMX_SetParameter(m_hEncoder,
                (OMX_INDEXTYPE) OMX_QcomIndexParamVideoMaxAllowedBitrateCheck,
                (OMX_PTR) &bitrate_check);
      }
#endif
#endif
      //////////////////////////////////////////
      // quantization
      //////////////////////////////////////////
      if (result == OMX_ErrorNone)
      {
        ///////////////////////////////////////////////////////////
        // QP Config
        ///////////////////////////////////////////////////////////
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE qp; // OMX_IndexParamVideoQuantization
        qp.nSize = sizeof(OMX_VIDEO_PARAM_QUANTIZATIONTYPE);
        qp.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
        result = OMX_GetParameter(m_hEncoder,
                                  OMX_IndexParamVideoQuantization,
                                  (OMX_PTR) &qp);
        if (result == OMX_ErrorNone)
        {
           if (m_eCodec == OMX_VIDEO_CodingAVC)
           {
              qp.nQpI = 30;
              qp.nQpP = 30;
           }
           else
           {
              qp.nQpI = 10;
              qp.nQpP = 10;
           }
           result = OMX_SetParameter(m_hEncoder,
                                     OMX_IndexParamVideoQuantization,
                                     (OMX_PTR) &qp);
        }
        else
        {
            // Failing to GetParameter will not be reported as fatal since the feature
            // may not be supported by the specific device.
            VENC_TEST_MSG_HIGH("Parameter not supported: OMX_IndexParamVideoQuantization");
            result = OMX_ErrorNone;
        }
     }

      //////////////////////////////////////////
      // quantization parameter range
      //////////////////////////////////////////
      if (result == OMX_ErrorNone)
      {
        ///////////////////////////////////////////////////////////
        // QP range setting
        ///////////////////////////////////////////////////////////
        OMX_QCOM_VIDEO_PARAM_QPRANGETYPE qprange; // OMX_QcomIndexParamVideoQPRange
        qprange.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
        result = OMX_GetParameter(m_hEncoder,
                                  (OMX_INDEXTYPE)OMX_QcomIndexParamVideoQPRange,
                                  (OMX_PTR) &qprange);
        if (result == OMX_ErrorNone)
        {
          qprange.minQP= pConfig->nMinQp;
          qprange.maxQP= pConfig->nMaxQp;
          VENC_TEST_MSG_MEDIUM("Original Config values with minQP = %d, maxQP = %d", qprange.minQP, qprange.maxQP,0);
          if (m_eCodec == OMX_VIDEO_CodingAVC)
          {
            if(qprange.minQP < 2)
              qprange.minQP = 2;
            if(qprange.maxQP > 51)
              qprange.maxQP = 51;
          }
          else
          {
            if(qprange.minQP < 2)
              qprange.minQP = 2;
            if(qprange.maxQP > 31)
              qprange.maxQP = 31;
          }
          VENC_TEST_MSG_MEDIUM("Calling OMX_QcomIndexParamVideoQPRange with minQP = %d, maxQP = %d", qprange.minQP, qprange.maxQP,0);
          qprange.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
          result = OMX_SetParameter(m_hEncoder,
                                    (OMX_INDEXTYPE)OMX_QcomIndexParamVideoQPRange,
                                    (OMX_PTR) &qprange);
        }
        else
        {
            // Failing to GetParameter will not be reported as fatal since the feature
            // may not be supported by the specific device.
            VENC_TEST_MSG_HIGH("Parameter not supported: OMX_QcomIndexParamVideoQPRange");
            result = OMX_ErrorNone;
        }
      }

      //////////////////////////////////////////
      // frame rate
      //////////////////////////////////////////
      if (result == OMX_ErrorNone)
      {
         OMX_CONFIG_FRAMERATETYPE framerate; // OMX_IndexConfigVideoFramerate
         framerate.nPortIndex = (OMX_U32) PORT_INDEX_IN; // input
         result = OMX_GetConfig(m_hEncoder,
                                OMX_IndexConfigVideoFramerate,
                                (OMX_PTR) &framerate);
         if (result == OMX_ErrorNone)
         {
            FractionToQ16(framerate.xEncodeFramerate,(int) (pConfig->nFramerate * 2),2);
            result = OMX_SetConfig(m_hEncoder,
                                   OMX_IndexConfigVideoFramerate,
                                   (OMX_PTR) &framerate);
         }
      }

      //////////////////////////////////////////
      // intra refresh
      //////////////////////////////////////////
      if (result == OMX_ErrorNone)
      {
         OMX_VIDEO_PARAM_INTRAREFRESHTYPE ir; // OMX_IndexParamVideoIntraRefresh
         ir.nSize = sizeof(OMX_VIDEO_PARAM_INTRAREFRESHTYPE);
         ir.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
         result = OMX_GetParameter(m_hEncoder,
                                   OMX_IndexParamVideoIntraRefresh,
                                   (OMX_PTR) &ir);
         if (result == OMX_ErrorNone)
         {
            if ((pConfig->nIntraRefreshMBCount < ((pConfig->nFrameWidth * pConfig->nFrameHeight) >> 8)))
            {
               ir.eRefreshMode = OMX_VIDEO_IntraRefreshCyclic;
               ir.nCirMBs = pConfig->nIntraRefreshMBCount;
               result = OMX_SetParameter(m_hEncoder,
                                         OMX_IndexParamVideoIntraRefresh,
                                         (OMX_PTR) &ir);
            }
            else
               VENC_TEST_MSG_ERROR("IntraRefresh not set for %d MBs because total MBs is %d",
                                   ir.nCirMBs,(pConfig->nFrameWidth*pConfig->nFrameHeight>>8),0);
         }
      }
      //////////////////////////////////////////
      // rotation
      //////////////////////////////////////////
      if (result == OMX_ErrorNone)
      {
         OMX_CONFIG_ROTATIONTYPE framerotate; // OMX_IndexConfigCommonRotate
         framerotate.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
         result = OMX_GetConfig(m_hEncoder,
                                OMX_IndexConfigCommonRotate,
                                (OMX_PTR) &framerotate);
         if (result == OMX_ErrorNone)
         {

            framerotate.nRotation = pConfig->nRotation;

            result = OMX_SetConfig(m_hEncoder,
                                   OMX_IndexConfigCommonRotate,
                                   (OMX_PTR) &framerotate);
         }
      }

#ifndef MSM8974
#ifdef _ANDROID_
      //////////////////////////////////////////
      // SetSyntaxHeader
      //////////////////////////////////////////
      char prop_val[PROPERTY_VALUE_MAX] = {0};
      OMX_U32 getcodechdr = 0;
      property_get("vidc.venc.debug.codechdr", prop_val, "0");
      getcodechdr = atoi(prop_val);
      if ((result == OMX_ErrorNone) && (getcodechdr))
      {
         #define VIDEO_SYNTAXHDRSIZE 100
         VENC_TEST_MSG_HIGH("getcodechdr is enabled via setprop command");
         QOMX_EXTNINDEX_PARAMTYPE codechdr;
         memset(&codechdr, 0, sizeof(QOMX_EXTNINDEX_PARAMTYPE));
         codechdr.nSize = sizeof(QOMX_EXTNINDEX_PARAMTYPE) +
                          VIDEO_SYNTAXHDRSIZE;
         codechdr.nPortIndex = (OMX_U32)PORT_INDEX_OUT; // output
         codechdr.pData = (OMX_PTR)malloc(VIDEO_SYNTAXHDRSIZE);
         if (codechdr.pData == NULL)
         {
            VENC_TEST_MSG_ERROR("failed to allocate memory for syntax header");
            result = OMX_ErrorInsufficientResources;
         }
         if (result == OMX_ErrorNone)
         {
            result = OMX_GetParameter(m_hEncoder,
                                   (OMX_INDEXTYPE)QOMX_IndexParamVideoSyntaxHdr,
                                   (OMX_PTR)&codechdr);
            if (result == OMX_ErrorNone)
            {
              for (OMX_U32 i = 0; i < codechdr.nDataSize; i++)
              {
                 VENC_TEST_MSG_HIGH("header[%d] = %x", i,
                    *((OMX_U8*)codechdr.pData + i));
              }
            }
            else
            {
               VENC_TEST_MSG_ERROR("failed to get syntax header");
            }
            if (codechdr.pData)
               free(codechdr.pData);
         }
      }
#endif
#endif
      return result;
   } /* OMX_ERRORTYPE Encoder::Configure(EncoderConfigType* pConfig) */

    Encoder(OMX_HANDLETYPE hEncoder, OMX_VIDEO_CODINGTYPE eCodec,
            OMX_S32& nInputBuffers, OMX_S32& nOutputBuffers,
            OMX_S32& nInputBufferSize, OMX_S32& nOutputBufferSize) :
        m_hEncoder(hEncoder), m_eCodec(eCodec),
        m_nInputBuffers(nInputBuffers), m_nOutputBuffers(nOutputBuffers),
        m_nInputBufferSize(nInputBufferSize), m_nOutputBufferSize(nOutputBufferSize)
    {}

    OMX_HANDLETYPE m_hEncoder;
    OMX_VIDEO_CODINGTYPE m_eCodec;
    OMX_S32& m_nInputBuffers;
    OMX_S32& m_nOutputBuffers;
    OMX_S32& m_nInputBufferSize;
    OMX_S32& m_nOutputBufferSize;
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE Configure(OMX_HANDLETYPE hEncoder,
                        OMX_VIDEO_CODINGTYPE eCodec,
                        OMX_S32& nInputBuffers, OMX_S32& nOutputBuffers,
                        OMX_S32& nInputBufferSize, OMX_S32& nOutputBufferSize,
                        EncoderConfigType& Config)
{
    Encoder e(hEncoder, eCodec, nInputBuffers, nOutputBuffers,
              nInputBufferSize, nOutputBufferSize);

    return e.Configure(&Config);
}

}}}/* namespace omx::video::encoder */
