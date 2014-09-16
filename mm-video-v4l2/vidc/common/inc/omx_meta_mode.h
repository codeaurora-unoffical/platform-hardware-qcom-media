/*
* Copyright (c) 2011-2014 The Linux Foundation.  All Rights Reserved.
*
* Not a Contribution, Apache license notifications and license are retained
* for attribution purposes only.
*/
/*
* Copyright (C) 2008-2009,2011 The Android Open Source Project
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef _OMX_METAMODE_H_
#define _OMX_METAMODE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <OMX_Types.h>

#define HAL_PIXEL_FORMAT_RGBA_8888  1
#define HAL_PIXEL_FORMAT_NV12_ENCODEABLE 0x102

struct native_handle
{
    int version;        /* sizeof(native_handle_t) */
    int numFds;         /* number of file-descriptors at &data[0] */
    int numInts;        /* number of ints at &data[numFds] */
    int data[0];        /* numFds + numInts ints */
};
typedef native_handle *buffer_handle;

enum MetaBufferType {
   kMetadataBufferTypeCameraSource  = 0,
   kMetadataBufferTypeGrallocSource = 1,
};

struct encoder_media_buffer_type {
   MetaBufferType buffer_type;
   buffer_handle meta_handle;
};

struct StoreMetaDataInBuffersParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bStoreMetaData;
};

#ifdef __cplusplus
}
#endif


#endif // _OMX_METAMODE_H_
