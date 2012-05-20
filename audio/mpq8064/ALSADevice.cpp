/* ALSADevice.cpp
 **
 ** Copyright 2009 Wind River Systems
 ** Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "ALSADevice"
//#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#include <utils/Log.h>
#include <cutils/properties.h>
#include <linux/ioctl.h>
#include "AudioHardwareALSA.h"
#include <media/AudioRecord.h>

#define BTSCO_RATE_16KHZ 16000
#define USECASE_TYPE_RX 1
#define USECASE_TYPE_TX 2

namespace android_audio_legacy
{

ALSADevice::ALSADevice() {
    mDevSettingsFlag = TTY_OFF;
    btsco_samplerate = 8000;
    int callMode = AudioSystem::MODE_NORMAL;

    char value[128];
    property_get("persist.audio.handset.mic",value,"0");
    strlcpy(mic_type, value, sizeof(mic_type));
    property_get("persist.audio.fluence.mode",value,"0");
    if (!strcmp("broadside", value)) {
        fluence_mode = FLUENCE_MODE_BROADSIDE;
    } else {
        fluence_mode = FLUENCE_MODE_ENDFIRE;
    }
    strlcpy(curRxUCMDevice, "None", sizeof(curRxUCMDevice));
    strlcpy(curTxUCMDevice, "None", sizeof(curTxUCMDevice));

    mMixer = mixer_open("/dev/snd/controlC0");

    LOGD("ALSA Device opened");
};

ALSADevice::~ALSADevice()
{
    if (mMixer) mixer_close(mMixer);
}

// ----------------------------------------------------------------------------

int ALSADevice::deviceName(alsa_handle_t *handle, unsigned flags, char **value)
{
    int ret = 0;
    char ident[70];
    char *rxDevice, useCase[70];

    if (flags & PCM_IN) {
        strlcpy(ident, "CapturePCM/", sizeof(ident));
    } else {
        strlcpy(ident, "PlaybackPCM/", sizeof(ident));
    }
    if((!strncmp(handle->useCase,SND_USE_CASE_VERB_MI2S,
                          strlen(SND_USE_CASE_VERB_MI2S)) ||
        !strncmp(handle->useCase,SND_USE_CASE_MOD_PLAY_MI2S,
                   strlen(SND_USE_CASE_MOD_PLAY_MI2S))) && !(flags & PCM_IN) ) {
        rxDevice = getUCMDevice(handle->devices & AudioSystem::DEVICE_OUT_ALL, 0);
        strlcpy(useCase,handle->useCase,sizeof(handle->useCase)+1);
        if(rxDevice) {
           strlcat(useCase,rxDevice,sizeof(useCase));
        }
        strlcat(ident, useCase, sizeof(ident));
        free(rxDevice);
    } else {
        strlcat(ident, handle->useCase, sizeof(ident));
    }
    ret = snd_use_case_get(handle->ucMgr, ident, (const char **)value);
    LOGD("Device value returned is %s", (*value));
    return ret;
}

status_t ALSADevice::setHardwareParams(alsa_handle_t *handle)
{
    struct snd_pcm_hw_params *params;
    unsigned long bufferSize, reqBuffSize;
    unsigned int periodTime, bufferTime;
    unsigned int requestedRate = handle->sampleRate;
    int status = 0;
    struct snd_compr_caps compr_cap;
    struct snd_compr_params compr_params;
    int format = handle->format;
    int32_t minPeroid, maxPeroid;
    status_t err = NO_ERROR;

    reqBuffSize = handle->bufferSize;

    if ((!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI_TUNNEL) ||
        (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_TUNNEL)))) {
        if (ioctl(handle->handle->fd, SNDRV_COMPRESS_GET_CAPS, &compr_cap)) {
            LOGE("SNDRV_COMPRESS_GET_CAPS, failed Error no %d \n", errno);
            err = -errno;
            return err;
        }

        minPeroid = compr_cap.min_fragment_size;
        maxPeroid = compr_cap.max_fragment_size;
        LOGV("Min peroid size = %d , Maximum Peroid size = %d",\
            minPeroid, maxPeroid);
        //TODO: what if codec not supported or the array has wrong codec!!!!
        if (format == AUDIO_FORMAT_WMA || format == AUDIO_FORMAT_WMA_PRO) {
            LOGV("### WMA CODEC");
            if (format == AUDIO_FORMAT_WMA_PRO) {
                compr_params.codec.id = compr_cap.codecs[4];
                compr_params.codec.ch_in = handle->channels;
                if (handle->channels > 2)
                    handle->channels = 2;
            }
            else {
                compr_params.codec.id = compr_cap.codecs[3];
            }
            if (mWMA_params == NULL) {
                LOGV("WMA param config missing.");
                return BAD_VALUE;
            }
            compr_params.codec.bit_rate = mWMA_params[0];
            compr_params.codec.align = mWMA_params[1];
            compr_params.codec.options.wma.encodeopt = mWMA_params[2];
            compr_params.codec.format = mWMA_params[3];
            compr_params.codec.options.wma.bits_per_sample = mWMA_params[4];
            compr_params.codec.options.wma.channelmask = mWMA_params[5];
        } else if (format == AUDIO_FORMAT_AAC) {
            LOGV("### AAC CODEC");
            compr_params.codec.id = compr_cap.codecs[1];
        }
        else {
             LOGW("### MP3 CODEC");
             compr_params.codec.id = compr_cap.codecs[0];
        }
        if (ioctl(handle->handle->fd, SNDRV_COMPRESS_SET_PARAMS, &compr_params)) {
            LOGE("SNDRV_COMPRESS_SET_PARAMS,failed Error no %d \n", errno);
            err = -errno;
            return err;
        }
    }

    params = (snd_pcm_hw_params*) calloc(1, sizeof(struct snd_pcm_hw_params));
    if (!params) {
		//SMANI:: Commented to fix build issues. FIX IT.
        //LOG_ALWAYS_FATAL("Failed to allocate ALSA hardware parameters!");
        return NO_INIT;
    }

    LOGD("setHardwareParams: reqBuffSize %d channels %d sampleRate %d",
         (int) reqBuffSize, handle->channels, handle->sampleRate);

    param_init(params);
    param_set_mask(params, SNDRV_PCM_HW_PARAM_ACCESS,
        (handle->handle->flags & PCM_MMAP) ? SNDRV_PCM_ACCESS_MMAP_INTERLEAVED
        : SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
                   SNDRV_PCM_FORMAT_S16_LE);
    param_set_mask(params, SNDRV_PCM_HW_PARAM_SUBFORMAT,
                   SNDRV_PCM_SUBFORMAT_STD);
    LOGV("hw params -  before hifi2 condition %s", handle->useCase);

    if ((!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI2)) ||
        (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_MUSIC2))) {
        int ALSAbufferSize = getALSABufferSize(handle);
        param_set_int(params, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, ALSAbufferSize);
        LOGD("ALSAbufferSize = %d",ALSAbufferSize);
        param_set_int(params, SNDRV_PCM_HW_PARAM_PERIODS, MULTI_CHANNEL_PERIOD_COUNT);
    }
    else {
        param_set_min(params, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, reqBuffSize);
    }

    param_set_int(params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 16);
    param_set_int(params, SNDRV_PCM_HW_PARAM_FRAME_BITS,
                   handle->channels - 1 ? 32 : 16);
    param_set_int(params, SNDRV_PCM_HW_PARAM_CHANNELS,
                  handle->channels);
    param_set_int(params, SNDRV_PCM_HW_PARAM_RATE, handle->sampleRate);
    param_set_hw_refine(handle->handle, params);

    if (param_set_hw_params(handle->handle, params)) {
        LOGE("cannot set hw params");
        return NO_INIT;
    }
    param_dump(params);

    handle->handle->buffer_size = pcm_buffer_size(params);
    handle->handle->period_size = pcm_period_size(params);
    handle->handle->period_cnt = handle->handle->buffer_size/handle->handle->period_size;
    LOGD("setHardwareParams: buffer_size %d, period_size %d, period_cnt %d",
        handle->handle->buffer_size, handle->handle->period_size,
        handle->handle->period_cnt);
    handle->handle->rate = handle->sampleRate;
    handle->handle->channels = handle->channels;
    handle->periodSize = handle->handle->period_size;
    handle->bufferSize = handle->handle->period_size;
    if ((!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI2)) ||
        (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_MUSIC2))) {
        handle->latency += (handle->handle->period_cnt * PCM_BUFFER_DURATION);
    }
    return NO_ERROR;
}

status_t ALSADevice::setSoftwareParams(alsa_handle_t *handle)
{
    struct snd_pcm_sw_params* params;
    struct pcm* pcm = handle->handle;

    unsigned long periodSize = pcm->period_size;

    params = (snd_pcm_sw_params*) calloc(1, sizeof(struct snd_pcm_sw_params));
    if (!params) {
        LOG_ALWAYS_FATAL("Failed to allocate ALSA software parameters!");
        return NO_INIT;
    }

    // Get the current software parameters
    params->tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
    params->period_step = 1;
    if(((!strcmp(handle->useCase,SND_USE_CASE_MOD_PLAY_VOIP)) ||
        (!strcmp(handle->useCase,SND_USE_CASE_VERB_IP_VOICECALL)))){
          LOGV("setparam:  start & stop threshold for Voip ");
          params->avail_min = handle->channels - 1 ? periodSize/4 : periodSize/2;
          params->start_threshold = periodSize/2;
          params->stop_threshold = INT_MAX;
     } else {
         params->avail_min = periodSize/2;
         params->start_threshold = handle->channels - 1 ? periodSize/2 : periodSize/4;
         params->stop_threshold = INT_MAX;
     }
    if (!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI_TUNNEL) ||
        (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_TUNNEL))) {
        params->tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
        params->period_step = 1;
        params->xfer_align = (handle->handle->flags & PCM_MONO) ?
            handle->handle->period_size/2 : handle->handle->period_size/4;
    }
    params->silence_threshold = 0;
    params->silence_size = 0;

    if (param_set_sw_params(handle->handle, params)) {
        LOGE("cannot set sw params");
        return NO_INIT;
    }
    return NO_ERROR;
}

void ALSADevice::switchDevice(alsa_handle_t *handle, uint32_t devices, uint32_t mode)
{
    const char **mods_list;
    use_case_t useCaseNode;
    unsigned usecase_type = 0;
    bool inCallDevSwitch = false;
    char *rxDevice, *txDevice, ident[70], *use_case = NULL;
    int err = 0, index, mods_size;
    LOGV("%s: device %d", __FUNCTION__, devices);

    if ((mode == AudioSystem::MODE_IN_CALL)  || (mode == AudioSystem::MODE_IN_COMMUNICATION)) {
        if ((devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) ||
            (devices & AudioSystem::DEVICE_IN_WIRED_HEADSET)) {
            devices = devices | (AudioSystem::DEVICE_OUT_WIRED_HEADSET |
                      AudioSystem::DEVICE_IN_WIRED_HEADSET);
        } else if (devices & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE) {
            devices = devices | (AudioSystem::DEVICE_OUT_WIRED_HEADPHONE |
                      AudioSystem::DEVICE_IN_BUILTIN_MIC);
        } else if ((devices & AudioSystem::DEVICE_OUT_EARPIECE) ||
                  (devices & AudioSystem::DEVICE_IN_BUILTIN_MIC)) {
            devices = devices | (AudioSystem::DEVICE_IN_BUILTIN_MIC |
                      AudioSystem::DEVICE_OUT_EARPIECE);
        } else if (devices & AudioSystem::DEVICE_OUT_SPEAKER) {
            devices = devices | (AudioSystem::DEVICE_IN_DEFAULT |
                       AudioSystem::DEVICE_OUT_SPEAKER);
        } else if ((devices & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO) ||
                   (devices & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET) ||
                   (devices & AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET)) {
            devices = devices | (AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET |
                      AudioSystem::DEVICE_OUT_BLUETOOTH_SCO);
        } else if ((devices & AudioSystem::DEVICE_OUT_ANC_HEADSET) ||
                   (devices & AudioSystem::DEVICE_IN_ANC_HEADSET)) {
            devices = devices | (AudioSystem::DEVICE_OUT_ANC_HEADSET |
                      AudioSystem::DEVICE_IN_ANC_HEADSET);
        } else if (devices & AudioSystem::DEVICE_OUT_ANC_HEADPHONE) {
            devices = devices | (AudioSystem::DEVICE_OUT_ANC_HEADPHONE |
                      AudioSystem::DEVICE_IN_BUILTIN_MIC);
        } else if (devices & AudioSystem::DEVICE_OUT_AUX_DIGITAL) {
            devices = devices | (AudioSystem::DEVICE_OUT_AUX_DIGITAL |
                      AudioSystem::DEVICE_IN_AUX_DIGITAL);
        } else if (devices & AudioSystem::DEVICE_OUT_PROXY) {
            devices = devices | (AudioSystem::DEVICE_OUT_PROXY |
                      AudioSystem::DEVICE_IN_BUILTIN_MIC);
        }
    }

    rxDevice = getUCMDevice(devices & AudioSystem::DEVICE_OUT_ALL, 0);
    txDevice = getUCMDevice(devices & AudioSystem::DEVICE_IN_ALL, 1);
    if ((rxDevice != NULL) && (txDevice != NULL)) {
        if (((strcmp(rxDevice, curRxUCMDevice)) || (strcmp(txDevice, curTxUCMDevice))) &&
            (mode == AudioSystem::MODE_IN_CALL))
            inCallDevSwitch = true;
    }

    snd_use_case_get(handle->ucMgr, "_verb", (const char **)&use_case);
    mods_size = snd_use_case_get_list(handle->ucMgr, "_enamods", &mods_list);
    if (rxDevice != NULL) {
        if ((strcmp(curRxUCMDevice, "None")) &&
	        ((strcmp(rxDevice, curRxUCMDevice)) || (inCallDevSwitch == true))) {
            if ((use_case != NULL) && (strncmp(use_case, SND_USE_CASE_VERB_INACTIVE,
	            strlen(SND_USE_CASE_VERB_INACTIVE)))) {
                usecase_type = getUseCaseType(use_case);
                if (usecase_type & USECASE_TYPE_RX) {
                    LOGD("Deroute use case %s type is %d\n", use_case, usecase_type);
                    strlcpy(useCaseNode.useCase, use_case, MAX_STR_LEN);
                    snd_use_case_set(handle->ucMgr, "_verb", SND_USE_CASE_VERB_INACTIVE);
                    mUseCaseList.push_front(useCaseNode);
                }
            }
            if (mods_size) {
                for(index = 0; index < mods_size; index++) {
                    usecase_type = getUseCaseType(mods_list[index]);
                    if (usecase_type & USECASE_TYPE_RX) {
                        LOGD("Deroute use case %s type is %d\n", mods_list[index], usecase_type);
                        strlcpy(useCaseNode.useCase, mods_list[index], MAX_STR_LEN);
                        snd_use_case_set(handle->ucMgr, "_dismod", mods_list[index]);
                        mUseCaseList.push_back(useCaseNode);
                    }
                }
            }
            snd_use_case_set(handle->ucMgr, "_disdev", curRxUCMDevice);
        }
    }
    if (txDevice != NULL) {
        if ((strcmp(curTxUCMDevice, "None")) &&
            ((strcmp(txDevice, curTxUCMDevice)) || (inCallDevSwitch == true))) {
            if ((use_case != NULL) && (strncmp(use_case, SND_USE_CASE_VERB_INACTIVE,
	                strlen(SND_USE_CASE_VERB_INACTIVE)))) {
                usecase_type = getUseCaseType(use_case);
                if ((usecase_type & USECASE_TYPE_TX) && (!(usecase_type & USECASE_TYPE_RX))) {
                   LOGD("Deroute use case %s type is %d\n", use_case, usecase_type);
                   strlcpy(useCaseNode.useCase, use_case, MAX_STR_LEN);
                   snd_use_case_set(handle->ucMgr, "_verb", SND_USE_CASE_VERB_INACTIVE);
                   mUseCaseList.push_front(useCaseNode);
                }
            }
            if (mods_size) {
                for(index = 0; index < mods_size; index++) {
                    usecase_type = getUseCaseType(mods_list[index]);
                    if ((usecase_type & USECASE_TYPE_TX) && (!(usecase_type & USECASE_TYPE_RX))) {
                        LOGD("Deroute use case %s type is %d\n", mods_list[index], usecase_type);
                        strlcpy(useCaseNode.useCase, mods_list[index], MAX_STR_LEN);
                        snd_use_case_set(handle->ucMgr, "_dismod", mods_list[index]);
                        mUseCaseList.push_back(useCaseNode);
                    }
                }
            }
            snd_use_case_set(handle->ucMgr, "_disdev", curTxUCMDevice);
       }
    }
    if (rxDevice != NULL) {
        snd_use_case_set(handle->ucMgr, "_enadev", rxDevice);
        strlcpy(curRxUCMDevice, rxDevice, sizeof(curRxUCMDevice));
        if (devices & AudioSystem::DEVICE_OUT_FM)
            setFmVolume(fmVolume);
        free(rxDevice);
    }
    if (txDevice != NULL) {
       snd_use_case_set(handle->ucMgr, "_enadev", txDevice);
       strlcpy(curTxUCMDevice, txDevice, sizeof(curTxUCMDevice));
       free(txDevice);
    }
    for(ALSAUseCaseList::iterator it = mUseCaseList.begin(); it != mUseCaseList.end(); ++it) {
        LOGE("Route use case %s\n", it->useCase);

        if ((use_case != NULL) && (strncmp(use_case, SND_USE_CASE_VERB_INACTIVE,
            strlen(SND_USE_CASE_VERB_INACTIVE))) && (!strncmp(use_case, it->useCase, MAX_UC_LEN))) {
            snd_use_case_set(handle->ucMgr, "_verb", it->useCase);
        } else {
            snd_use_case_set(handle->ucMgr, "_enamod", it->useCase);
        }
    }
    if (!mUseCaseList.empty())
        mUseCaseList.clear();
    if (use_case != NULL)
        free(use_case);
    LOGD("switchDevice: curTxUCMDevivce %s curRxDevDevice %s", curTxUCMDevice, curRxUCMDevice);

}

// ----------------------------------------------------------------------------

status_t ALSADevice::open(alsa_handle_t *handle)
{
    char *devName;
    unsigned flags = 0;
    int err = NO_ERROR;

    /* No need to call s_close for LPA as pcm device open and close is handled by LPAPlayer in stagefright */
    if((!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI_LOW_POWER)) || (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_LPA))) {
        LOGD("s_open: Opening LPA playback");
        return NO_ERROR;
    }

    close(handle);
    LOGD("s_open: handle %p", handle);

    if(handle->channels == 1 && handle->devices & AudioSystem::DEVICE_OUT_AUX_DIGITAL) {
        err = setHDMIChannelCount();
        if(err != OK) {
            LOGE("setHDMIChannelCount err = %d", err);
            return err;
        }
    }

    // ASoC multicomponent requires a valid path (frontend/backend) for
    // the device to be opened

    // The PCM stream is opened in blocking mode, per ALSA defaults.  The
    // AudioFlinger seems to assume blocking mode too, so asynchronous mode
    // should not be used.
    // ToDo: Add a condition check for HIFI2 use cases also
    if ((!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI)) ||
        (!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI2)) ||
        (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_MUSIC)) ||
        (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_MUSIC2)) ||
        (!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI_TUNNEL)) ||
        (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_TUNNEL))) {
        flags = PCM_OUT;
    } else {
        flags = PCM_IN;
    }

    if (handle->channels == 1) {
        flags |= PCM_MONO;
    } else if (handle->channels == 6) {
        flags |= PCM_5POINT1;
    }else {
        flags |= PCM_STEREO;
    }
    if (deviceName(handle, flags, &devName) < 0) {
        LOGE("Failed to get pcm device node: %s", devName);
        return NO_INIT;
    }
    if (!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI_TUNNEL) ||
        (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_TUNNEL))) {
        flags |= DEBUG_ON | PCM_MMAP;
    }
    handle->handle = pcm_open(flags, (char*)devName);
    LOGE("s_open: opening ALSA device '%s'", devName);
    if (!handle->handle) {
        LOGE("s_open: Failed to initialize ALSA device '%s'", devName);
        free(devName);
        return NO_INIT;
    }
    handle->handle->flags = flags;
    LOGD("setting hardware parameters");
    err = setHardwareParams(handle);
    if (err == NO_ERROR) {
        LOGD("setting software parameters");
        err = setSoftwareParams(handle);
    }
    if(err != NO_ERROR) {
        LOGE("Set HW/SW params failed: Closing the pcm stream");
        standby(handle);
        free(devName);
        return err;
    }

    free(devName);
    return NO_ERROR;
}

status_t ALSADevice::startVoipCall(alsa_handle_t *handle)
{

    char* devName;
    char* devName1;
    unsigned flags = 0;
    int err = NO_ERROR;
    uint8_t voc_pkt[VOIP_BUFFER_MAX_SIZE];

    close(handle);
    flags = PCM_OUT;
    flags |= PCM_MONO;
    LOGV("s_open:s_start_voip_call  handle %p", handle);

    if (deviceName(handle, flags, &devName) < 0) {
         LOGE("Failed to get pcm device node");
         return NO_INIT;
    }

     handle->handle = pcm_open(flags, (char*)devName);

     if (!handle->handle) {
          free(devName);
          LOGE("s_open: Failed to initialize ALSA device '%s'", devName);
          return NO_INIT;
     }

     if (!pcm_ready(handle->handle)) {
         LOGE(" pcm ready failed");
     }

     handle->handle->flags = flags;
     err = setHardwareParams(handle);

     if (err == NO_ERROR) {
         err = setSoftwareParams(handle);
     }

     err = pcm_prepare(handle->handle);
     if(err != NO_ERROR) {
         LOGE("DEVICE_OUT_DIRECTOUTPUT: pcm_prepare failed");
     }

     /* first write required start dsp */
     memset(&voc_pkt,0,sizeof(voc_pkt));
     pcm_write(handle->handle,&voc_pkt,handle->handle->period_size);
     handle->rxHandle = handle->handle;
     free(devName);
     LOGV("s_open: DEVICE_IN_COMMUNICATION ");
     flags = PCM_IN;
     flags |= PCM_MONO;
     handle->handle = 0;

     if (deviceName(handle, flags, &devName1) < 0) {
        LOGE("Failed to get pcm device node");
        return NO_INIT;
     }
     handle->handle = pcm_open(flags, (char*)devName1);

     if (!handle->handle) {
         free(devName);
         LOGE("s_open: Failed to initialize ALSA device '%s'", devName);
         return NO_INIT;
     }

     if (!pcm_ready(handle->handle)) {
        LOGE(" pcm ready in failed");
     }

     handle->handle->flags = flags;
     err = setHardwareParams(handle);

     if (err == NO_ERROR) {
         err = setSoftwareParams(handle);
     }


     err = pcm_prepare(handle->handle);
     if(err != NO_ERROR) {
         LOGE("DEVICE_IN_COMMUNICATION: pcm_prepare failed");
     }

     /* first read required start dsp */
     memset(&voc_pkt,0,sizeof(voc_pkt));
     pcm_read(handle->handle,&voc_pkt,handle->handle->period_size);
     return NO_ERROR;
}

status_t ALSADevice::startVoiceCall(alsa_handle_t *handle)
{
    char* devName;
    unsigned flags = 0;
    int err = NO_ERROR;

    LOGD("startVoiceCall: handle %p", handle);
    // ASoC multicomponent requires a valid path (frontend/backend) for
    // the device to be opened

    flags = PCM_OUT | PCM_MONO;
    if (deviceName(handle, flags, &devName) < 0) {
        LOGE("Failed to get pcm device node");
        return NO_INIT;
    }
    handle->handle = pcm_open(flags, (char*)devName);
    if (!handle->handle) {
        LOGE("startVoiceCall: could not open PCM device");
        goto Error;
    }

    handle->handle->flags = flags;
    err = setHardwareParams(handle);
    if(err != NO_ERROR) {
        LOGE("startVoiceCall: setHardwareParams failed");
        goto Error;
    }

    err = setSoftwareParams(handle);
    if(err != NO_ERROR) {
        LOGE("startVoiceCall: setSoftwareParams failed");
        goto Error;
    }

    err = pcm_prepare(handle->handle);
    if(err != NO_ERROR) {
        LOGE("startVoiceCall: pcm_prepare failed");
        goto Error;
    }

    if (ioctl(handle->handle->fd, SNDRV_PCM_IOCTL_START)) {
        LOGE("startVoiceCall:SNDRV_PCM_IOCTL_START failed\n");
        goto Error;
    }

    // Store the PCM playback device pointer in rxHandle
    handle->rxHandle = handle->handle;
    free(devName);

    // Open PCM capture device
    flags = PCM_IN | PCM_MONO;
    if (deviceName(handle, flags, &devName) < 0) {
        LOGE("Failed to get pcm device node");
        goto Error;
    }
    handle->handle = pcm_open(flags, (char*)devName);
    if (!handle->handle) {
        free(devName);
        goto Error;
    }

    handle->handle->flags = flags;
    err = setHardwareParams(handle);
    if(err != NO_ERROR) {
        LOGE("startVoiceCall: setHardwareParams failed");
        goto Error;
    }

    err = setSoftwareParams(handle);
    if(err != NO_ERROR) {
        LOGE("startVoiceCall: setSoftwareParams failed");
        goto Error;
    }

    err = pcm_prepare(handle->handle);
    if(err != NO_ERROR) {
        LOGE("startVoiceCall: pcm_prepare failed");
        goto Error;
    }

    if (ioctl(handle->handle->fd, SNDRV_PCM_IOCTL_START)) {
        LOGE("startVoiceCall:SNDRV_PCM_IOCTL_START failed\n");
        goto Error;
    }

    free(devName);
    return NO_ERROR;

Error:
    LOGE("startVoiceCall: Failed to initialize ALSA device '%s'", devName);
    free(devName);
    close(handle);
    return NO_INIT;
}

status_t ALSADevice::startFm(alsa_handle_t *handle)
{
    int err = NO_ERROR;

    err = startLoopback(handle);

    if(err == NO_ERROR)
        setFmVolume(fmVolume);

    return err;
}

status_t ALSADevice::startLoopback(alsa_handle_t *handle)
{
    char *devName;
    unsigned flags = 0;
    int err = NO_ERROR;

    LOGE("s_start_fm: handle %p", handle);

    // ASoC multicomponent requires a valid path (frontend/backend) for
    // the device to be opened

    flags = PCM_OUT | PCM_STEREO;
    if (deviceName(handle, flags, &devName) < 0) {
        LOGE("Failed to get pcm device node");
        goto Error;
    }
    handle->handle = pcm_open(flags, (char*)devName);
    if (!handle->handle) {
        LOGE("s_start_fm: could not open PCM device");
        goto Error;
    }

    handle->handle->flags = flags;
    err = setHardwareParams(handle);
    if(err != NO_ERROR) {
        LOGE("s_start_fm: setHardwareParams failed");
        goto Error;
    }

    err = setSoftwareParams(handle);
    if(err != NO_ERROR) {
        LOGE("s_start_fm: setSoftwareParams failed");
        goto Error;
    }

    err = pcm_prepare(handle->handle);
    if(err != NO_ERROR) {
        LOGE("s_start_fm: setSoftwareParams failed");
        goto Error;
    }

    if (ioctl(handle->handle->fd, SNDRV_PCM_IOCTL_START)) {
        LOGE("s_start_fm: SNDRV_PCM_IOCTL_START failed\n");
        goto Error;
    }

    // Store the PCM playback device pointer in rxHandle
    handle->rxHandle = handle->handle;
    free(devName);

    // Open PCM capture device
    flags = PCM_IN | PCM_STEREO;
    if (deviceName(handle, flags, &devName) < 0) {
        LOGE("Failed to get pcm device node");
        goto Error;
    }
    handle->handle = pcm_open(flags, (char*)devName);
    if (!handle->handle) {
        goto Error;
    }

    handle->handle->flags = flags;
    err = setHardwareParams(handle);
    if(err != NO_ERROR) {
        LOGE("s_start_fm: setHardwareParams failed");
        goto Error;
    }

    err = setSoftwareParams(handle);
    if(err != NO_ERROR) {
        LOGE("s_start_fm: setSoftwareParams failed");
        goto Error;
    }

    err = pcm_prepare(handle->handle);
    if(err != NO_ERROR) {
        LOGE("s_start_fm: pcm_prepare failed");
        goto Error;
    }

    if (ioctl(handle->handle->fd, SNDRV_PCM_IOCTL_START)) {
        LOGE("s_start_fm: SNDRV_PCM_IOCTL_START failed\n");
        goto Error;
    }

    free(devName);
    return NO_ERROR;

Error:
    free(devName);
    close(handle);
    return NO_INIT;
}

status_t ALSADevice::setChannelStatus(unsigned char *channelStatus)
{
    struct snd_aes_iec958 iec958;
    status_t err = NO_ERROR;
    memcpy(iec958.status, channelStatus,24);
    unsigned int ptr = (unsigned int)&iec958;
    setMixerControl("IEC958 Playback PCM Stream",ptr,0);

    return err;
}

status_t ALSADevice::setFmVolume(int value)
{
    status_t err = NO_ERROR;
    setMixerControl("Internal FM RX Volume",value,0);
    fmVolume = value;

    return err;
}

status_t ALSADevice::setLpaVolume(int value)
{
    status_t err = NO_ERROR;
    setMixerControl("LPA RX Volume",value,0);

    return err;
}

status_t ALSADevice::start(alsa_handle_t *handle)
{
    status_t err = NO_ERROR;

    if(!handle->handle) {
        LOGE("No active PCM driver to start");
        return err;
    }

    err = pcm_prepare(handle->handle);

    return err;
}

status_t ALSADevice::close(alsa_handle_t *handle)
{
    int ret;
    status_t err = NO_ERROR;
     struct pcm *h = handle->rxHandle;

    handle->rxHandle = 0;
    LOGD("close: handle %p h %p", handle, h);
    if (h) {
        LOGV("close rxHandle\n");
        err = pcm_close(h);
        if(err != NO_ERROR) {
            LOGE("close: pcm_close failed for rxHandle with err %d", err);
        }
    }

    h = handle->handle;
    handle->handle = 0;

    if (h) {
          LOGV("close handle h %p\n", h);
        err = pcm_close(h);
        if(err != NO_ERROR) {
            LOGE("close: pcm_close failed for handle with err %d", err);
        }
        disableDevice(handle);
    } else if((!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI_LOW_POWER)) ||
              (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_LPA))) {
        disableDevice(handle);
    }

    return err;
}

/*
    this is same as close, but don't discard
    the device/mode info. This way we can still
    close the device, hit idle and power-save, reopen the pcm
    for the same device/mode after resuming
*/
status_t ALSADevice::standby(alsa_handle_t *handle)
{
    int ret;
    status_t err = NO_ERROR;  
    struct pcm *h = handle->rxHandle;
    handle->rxHandle = 0;
    LOGD("s_standby: handle %p h %p", handle, h);
    if (h) {
        LOGE("s_standby  rxHandle\n");
        err = pcm_close(h);
        if(err != NO_ERROR) {
            LOGE("s_standby: pcm_close failed for rxHandle with err %d", err);
        }
    }

    h = handle->handle;
    handle->handle = 0;

    if (h) {
          LOGE("s_standby handle h %p\n", h);
        err = pcm_close(h);
        if(err != NO_ERROR) {
            LOGE("s_standby: pcm_close failed for handle with err %d", err);
        }
        disableDevice(handle);
    } else if((!strcmp(handle->useCase, SND_USE_CASE_VERB_HIFI_LOW_POWER)) ||
              (!strcmp(handle->useCase, SND_USE_CASE_MOD_PLAY_LPA))) {
        disableDevice(handle);
    }

    return err;
}

status_t ALSADevice::route(alsa_handle_t *handle, uint32_t devices, int mode)
{
    status_t status = NO_ERROR;

    LOGD("s_route: devices 0x%x in mode %d", devices, mode);
    callMode = mode;
    switchDevice(handle, devices, mode);
    return status;
}

int  ALSADevice::getUseCaseType(const char *useCase)
{
    LOGE("use case is %s\n", useCase);
    if (!strncmp(useCase, SND_USE_CASE_VERB_HIFI,
           strlen(SND_USE_CASE_VERB_HIFI)) ||
        !strncmp(useCase, SND_USE_CASE_VERB_HIFI_LOW_POWER,
           strlen(SND_USE_CASE_VERB_HIFI_LOW_POWER)) ||
        !strncmp(useCase, SND_USE_CASE_VERB_HIFI_TUNNEL,
           strlen(SND_USE_CASE_VERB_HIFI_TUNNEL)) ||
        !strncmp(useCase, SND_USE_CASE_VERB_HIFI2,
           strlen(SND_USE_CASE_VERB_HIFI2)) ||
        !strncmp(useCase, SND_USE_CASE_VERB_DIGITAL_RADIO,
           strlen(SND_USE_CASE_VERB_DIGITAL_RADIO)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_PLAY_MUSIC,
           strlen(SND_USE_CASE_MOD_PLAY_MUSIC)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_PLAY_LPA,
           strlen(SND_USE_CASE_MOD_PLAY_LPA)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_PLAY_TUNNEL,
           strlen(SND_USE_CASE_MOD_PLAY_TUNNEL)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_PLAY_MUSIC2,
           strlen(SND_USE_CASE_MOD_PLAY_MUSIC2)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_PLAY_FM,
           strlen(SND_USE_CASE_MOD_PLAY_FM))) {
        return USECASE_TYPE_RX;
    } else if (!strncmp(useCase, SND_USE_CASE_VERB_HIFI_REC,
           strlen(SND_USE_CASE_VERB_HIFI_REC)) ||
        !strncmp(useCase, SND_USE_CASE_VERB_FM_REC,
           strlen(SND_USE_CASE_VERB_FM_REC)) ||
        !strncmp(useCase, SND_USE_CASE_VERB_FM_A2DP_REC,
           strlen(SND_USE_CASE_VERB_FM_A2DP_REC)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_CAPTURE_MUSIC,
           strlen(SND_USE_CASE_MOD_CAPTURE_MUSIC)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_CAPTURE_FM,
           strlen(SND_USE_CASE_MOD_CAPTURE_FM)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_CAPTURE_A2DP_FM,
           strlen(SND_USE_CASE_MOD_CAPTURE_A2DP_FM))) {
        return USECASE_TYPE_TX;
    } else if (!strncmp(useCase, SND_USE_CASE_VERB_VOICECALL,
           strlen(SND_USE_CASE_VERB_VOICECALL)) ||
        !strncmp(useCase, SND_USE_CASE_VERB_IP_VOICECALL,
           strlen(SND_USE_CASE_VERB_IP_VOICECALL)) ||
        !strncmp(useCase, SND_USE_CASE_VERB_DL_REC,
           strlen(SND_USE_CASE_VERB_DL_REC)) ||
        !strncmp(useCase, SND_USE_CASE_VERB_UL_DL_REC,
           strlen(SND_USE_CASE_VERB_UL_DL_REC)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_PLAY_VOICE,
           strlen(SND_USE_CASE_MOD_PLAY_VOICE)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_PLAY_VOIP,
           strlen(SND_USE_CASE_MOD_PLAY_VOIP)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_CAPTURE_VOICE_DL,
           strlen(SND_USE_CASE_MOD_CAPTURE_VOICE_DL)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_CAPTURE_VOICE_UL_DL,
           strlen(SND_USE_CASE_MOD_CAPTURE_VOICE_UL_DL)) ||
        !strncmp(useCase, SND_USE_CASE_VERB_MI2S,
           strlen(SND_USE_CASE_VERB_MI2S)) ||
        !strncmp(useCase, SND_USE_CASE_MOD_PLAY_MI2S,
           strlen(SND_USE_CASE_MOD_PLAY_MI2S))) {
        return (USECASE_TYPE_RX | USECASE_TYPE_TX);
  } else {
       LOGE("unknown use case %s\n", useCase);
        return 0;
  }
}

void ALSADevice::disableDevice(alsa_handle_t *handle)
{
    unsigned usecase_type = 0;
    int i, mods_size;
    char *useCase;
    const char **mods_list;
    snd_use_case_get(handle->ucMgr, "_verb", (const char **)&useCase);
    if (useCase != NULL) {
        if (!strncmp(useCase, handle->useCase, MAX_UC_LEN)) {
            snd_use_case_set(handle->ucMgr, "_verb", SND_USE_CASE_VERB_INACTIVE);
        } else {
            snd_use_case_set(handle->ucMgr, "_dismod", handle->useCase);
        }
        free(useCase);
        snd_use_case_get(handle->ucMgr, "_verb", (const char **)&useCase);
        if (strncmp(useCase, SND_USE_CASE_VERB_INACTIVE,
               strlen(SND_USE_CASE_VERB_INACTIVE)))
            usecase_type |= getUseCaseType(useCase);
        mods_size = snd_use_case_get_list(handle->ucMgr, "_enamods", &mods_list);
        LOGE("Number of modifiers %d\n", mods_size);
        if (mods_size) {
            for(i = 0; i < mods_size; i++) {
                LOGE("index %d modifier %s\n", i, mods_list[i]);
                usecase_type |= getUseCaseType(mods_list[i]);
            }
        }
        LOGE("usecase_type is %d\n", usecase_type);
        if (!(usecase_type & USECASE_TYPE_TX) && (strncmp(curTxUCMDevice, "None", 4)))
            snd_use_case_set(handle->ucMgr, "_disdev", curTxUCMDevice);
        if (!(usecase_type & USECASE_TYPE_RX) && (strncmp(curRxUCMDevice, "None", 4)))
            snd_use_case_set(handle->ucMgr, "_disdev", curRxUCMDevice);
    } else {
        LOGE("Invalid state, no valid use case found to disable");
    }
    free(useCase);
}

char* ALSADevice::getUCMDevice(uint32_t devices, int input)
{
    if (!input) {
        if (!(mDevSettingsFlag & TTY_OFF) &&
            (callMode == AudioSystem::MODE_IN_CALL) &&
            ((devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) ||
             (devices & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE) ||
             (devices & AudioSystem::DEVICE_OUT_ANC_HEADSET) ||
             (devices & AudioSystem::DEVICE_OUT_ANC_HEADPHONE))) {
             if (mDevSettingsFlag & TTY_VCO) {
                 return strdup(SND_USE_CASE_DEV_TTY_HEADSET_RX);
             } else if (mDevSettingsFlag & TTY_FULL) {
                 return strdup(SND_USE_CASE_DEV_TTY_FULL_RX);
             } else if (mDevSettingsFlag & TTY_HCO) {
                 return strdup(SND_USE_CASE_DEV_EARPIECE); /* HANDSET RX */
             }
        } else if( (devices & AudioSystem::DEVICE_OUT_SPEAKER) &&
                   (devices & AudioSystem::DEVICE_OUT_SPDIF) &&
                   ((devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) ||
                    (devices & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE) ) ) {
            if (mDevSettingsFlag & ANC_FLAG) {
                return strdup(SND_USE_CASE_DEV_SPDIF_SPEAKER_ANC_HEADSET);
            } else {
                return strdup(SND_USE_CASE_DEV_SPDIF_SPEAKER_HEADSET);
            }
        } else if ((devices & AudioSystem::DEVICE_OUT_SPEAKER) &&
            ((devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) ||
            (devices & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE))) {
            if (mDevSettingsFlag & ANC_FLAG) {
                return strdup(SND_USE_CASE_DEV_SPEAKER_ANC_HEADSET); /* COMBO SPEAKER+ANC HEADSET RX */
            } else {
                return strdup(SND_USE_CASE_DEV_SPEAKER_HEADSET); /* COMBO SPEAKER+HEADSET RX */
            }
        } else if ((devices & AudioSystem::DEVICE_OUT_SPDIF) &&
                   ((devices & AudioSystem::DEVICE_OUT_ANC_HEADSET)||
                    (devices & AudioSystem::DEVICE_OUT_ANC_HEADPHONE)) ) {
            return strdup(SND_USE_CASE_DEV_SPDIF_ANC_HEADSET);
        } else if ((devices & AudioSystem::DEVICE_OUT_SPEAKER) &&
            ((devices & AudioSystem::DEVICE_OUT_ANC_HEADSET) ||
            (devices & AudioSystem::DEVICE_OUT_ANC_HEADPHONE))) {
            return strdup(SND_USE_CASE_DEV_SPEAKER_ANC_HEADSET); /* COMBO SPEAKER+ANC HEADSET RX */
        } else if ((devices & AudioSystem::DEVICE_OUT_SPEAKER) &&
                 (devices & AudioSystem::DEVICE_OUT_FM_TX)) {
            return strdup(SND_USE_CASE_DEV_SPEAKER_FM_TX); /* COMBO SPEAKER+FM_TX RX */
        } else if ((devices & AudioSystem::DEVICE_OUT_SPEAKER) &&
                 (devices & AudioSystem::DEVICE_OUT_SPDIF)) {
            return strdup(SND_USE_CASE_DEV_SPDIF_SPEAKER); /* COMBO SPEAKER+FM_TX RX */
        } else if ((devices & AudioSystem::DEVICE_OUT_EARPIECE) &&
                 (devices & AudioSystem::DEVICE_OUT_SPDIF)) {
            return strdup(SND_USE_CASE_DEV_SPDIF_HANDSET); /* COMBO SPEAKER+FM_TX RX */
        } else if (devices & AudioSystem::DEVICE_OUT_EARPIECE) {
            return strdup(SND_USE_CASE_DEV_EARPIECE); /* HANDSET RX */
        } else if (devices & AudioSystem::DEVICE_OUT_SPEAKER) {
            return strdup(SND_USE_CASE_DEV_SPEAKER); /* SPEAKER RX */
        } else if ((devices & AudioSystem::DEVICE_OUT_SPDIF) &&
                   ((devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) ||
                    (devices & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE))) {
            if (mDevSettingsFlag & ANC_FLAG) {
                return strdup(SND_USE_CASE_DEV_SPDIF_ANC_HEADSET);
            } else {
                return strdup(SND_USE_CASE_DEV_SPDIF_HEADSET);
            }
        } else if ((devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) ||
                   (devices & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE)) {
            if (mDevSettingsFlag & ANC_FLAG) {
                return strdup(SND_USE_CASE_DEV_ANC_HEADSET); /* ANC HEADSET RX */
            } else {
                return strdup(SND_USE_CASE_DEV_HEADPHONES); /* HEADSET RX */
            }
        } else if ((devices & AudioSystem::DEVICE_OUT_ANC_HEADSET) ||
                   (devices & AudioSystem::DEVICE_OUT_ANC_HEADPHONE)) {
            return strdup(SND_USE_CASE_DEV_ANC_HEADSET); /* ANC HEADSET RX */
        } else if ((devices & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO) ||
                  (devices & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET) ||
                  (devices & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)) {
            if (btsco_samplerate == BTSCO_RATE_16KHZ)
                return strdup(SND_USE_CASE_DEV_BTSCO_WB_RX); /* BTSCO RX*/
            else
                return strdup(SND_USE_CASE_DEV_BTSCO_NB_RX); /* BTSCO RX*/
        } else if ((devices & AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP) ||
                   (devices & AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES) ||
                   (devices & AudioSystem::DEVICE_OUT_DIRECTOUTPUT) ||
                   (devices & AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER)) {
            /* Nothing to be done, use current active device */
            return strdup(curRxUCMDevice);
        } else if ((devices & AudioSystem::DEVICE_OUT_SPDIF) &&
                   (devices & AudioSystem::DEVICE_OUT_SPEAKER)) {
            return strdup(SND_USE_CASE_DEV_SPDIF_SPEAKER);
        } else if ((devices & AudioSystem::DEVICE_OUT_SPDIF) &&
                   (devices & AudioSystem::DEVICE_OUT_AUX_DIGITAL)) {
            return strdup(SND_USE_CASE_DEV_HDMI_SPDIF);
        } else if (devices & AudioSystem::DEVICE_OUT_AUX_DIGITAL) {
            return strdup(SND_USE_CASE_DEV_HDMI); /* HDMI RX */
        } else if (devices & AudioSystem::DEVICE_OUT_PROXY) {
            return strdup(SND_USE_CASE_DEV_PROXY_RX); /* PROXY RX */
        } else if (devices & AudioSystem::DEVICE_OUT_FM_TX) {
            return strdup(SND_USE_CASE_DEV_FM_TX); /* FM Tx */
        } else if (devices & AudioSystem::DEVICE_OUT_SPDIF) {
            return strdup(SND_USE_CASE_DEV_SPDIF);
        } else if (devices & AudioSystem::DEVICE_OUT_DEFAULT) {
            return strdup(SND_USE_CASE_DEV_SPEAKER); /* SPEAKER RX */
        } else {
            LOGD("No valid output device: %u", devices);
        }
    } else {
        if (!(mDevSettingsFlag & TTY_OFF) &&
            (callMode == AudioSystem::MODE_IN_CALL) &&
            ((devices & AudioSystem::DEVICE_IN_WIRED_HEADSET) ||
             (devices & AudioSystem::DEVICE_IN_ANC_HEADSET))) {
             if (mDevSettingsFlag & TTY_HCO) {
                 return strdup(SND_USE_CASE_DEV_TTY_HEADSET_TX);
             } else if (mDevSettingsFlag & TTY_FULL) {
                 return strdup(SND_USE_CASE_DEV_TTY_FULL_TX);
             } else if (mDevSettingsFlag & TTY_VCO) {
                 if (!strncmp(mic_type, "analog", 6)) {
                     return strdup(SND_USE_CASE_DEV_HANDSET); /* HANDSET TX */
                 } else {
                     return strdup(SND_USE_CASE_DEV_LINE); /* BUILTIN-MIC TX */
                 }
             }
        } else if (devices & AudioSystem::DEVICE_IN_BUILTIN_MIC) {
            if (!strncmp(mic_type, "analog", 6)) {
                return strdup(SND_USE_CASE_DEV_HANDSET); /* HANDSET TX */
            } else {
                if (mDevSettingsFlag & DMIC_FLAG) {
                    if (fluence_mode == FLUENCE_MODE_ENDFIRE) {
                        return strdup(SND_USE_CASE_DEV_DUAL_MIC_ENDFIRE); /* DUALMIC EF TX */
                    } else if (fluence_mode == FLUENCE_MODE_BROADSIDE) {
                        return strdup(SND_USE_CASE_DEV_DUAL_MIC_BROADSIDE); /* DUALMIC BS TX */
                    }
                } else if (mDevSettingsFlag & QMIC_FLAG){
                    return strdup(SND_USE_CASE_DEV_QUAD_MIC);
                } else {
                    return strdup(SND_USE_CASE_DEV_LINE); /* BUILTIN-MIC TX */
                }
            }
        } else if (devices & AudioSystem::DEVICE_IN_AUX_DIGITAL) {
            return strdup(SND_USE_CASE_DEV_HDMI_TX); /* HDMI TX */
        } else if ((devices & AudioSystem::DEVICE_IN_WIRED_HEADSET) ||
                   (devices & AudioSystem::DEVICE_IN_ANC_HEADSET)) {
            return strdup(SND_USE_CASE_DEV_HEADSET); /* HEADSET TX */
        } else if (devices & AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
             if (btsco_samplerate == BTSCO_RATE_16KHZ)
                 return strdup(SND_USE_CASE_DEV_BTSCO_WB_TX); /* BTSCO TX*/
             else
                 return strdup(SND_USE_CASE_DEV_BTSCO_NB_TX); /* BTSCO TX*/
        } else if (devices & AudioSystem::DEVICE_IN_DEFAULT) {
            if (!strncmp(mic_type, "analog", 6)) {
                return strdup(SND_USE_CASE_DEV_HANDSET); /* HANDSET TX */
            } else {
                if (mDevSettingsFlag & DMIC_FLAG) {
                    if (fluence_mode == FLUENCE_MODE_ENDFIRE) {
                        return strdup(SND_USE_CASE_DEV_SPEAKER_DUAL_MIC_ENDFIRE); /* DUALMIC EF TX */
                    } else if (fluence_mode == FLUENCE_MODE_BROADSIDE) {
                        return strdup(SND_USE_CASE_DEV_SPEAKER_DUAL_MIC_BROADSIDE); /* DUALMIC BS TX */
                    }
                } else if (mDevSettingsFlag & QMIC_FLAG){
                    return strdup(SND_USE_CASE_DEV_QUAD_MIC);
                } else {
                    return strdup(SND_USE_CASE_DEV_LINE); /* BUILTIN-MIC TX */
                }
            }
        } else if ((devices & AudioSystem::DEVICE_IN_COMMUNICATION) ||
                   (devices & AudioSystem::DEVICE_IN_FM_RX) ||
                   (devices & AudioSystem::DEVICE_IN_FM_RX_A2DP) ||
                   (devices & AudioSystem::DEVICE_IN_VOICE_CALL)) {
            /* Nothing to be done, use current active device */
            return strdup(curTxUCMDevice);
        } else if ((devices & AudioSystem::DEVICE_IN_COMMUNICATION) ||
                   (devices & AudioSystem::DEVICE_IN_AMBIENT) ||
                   (devices & AudioSystem::DEVICE_IN_BACK_MIC) ||
                   (devices & AudioSystem::DEVICE_IN_AUX_DIGITAL)) {
            LOGI("No proper mapping found with UCM device list, setting default");
            if (!strncmp(mic_type, "analog", 6)) {
                return strdup(SND_USE_CASE_DEV_HANDSET); /* HANDSET TX */
            } else {
                return strdup(SND_USE_CASE_DEV_LINE); /* BUILTIN-MIC TX */
            }
        } else {
            LOGD("No valid input device: %u", devices);
        }
    }
    return NULL;
}

void ALSADevice::setVoiceVolume(int vol)
{
    LOGD("setVoiceVolume: volume %d", vol);
    setMixerControl("Voice Rx Volume", vol, 0);
}

void ALSADevice::setVoipVolume(int vol)
{
    LOGD("setVoipVolume: volume %d", vol);
    setMixerControl("Voip Rx Volume", vol, 0);
}
void ALSADevice::setMicMute(int state)
{
    LOGD("setMicMute: state %d", state);
    setMixerControl("Voice Tx Mute", state, 0);
}

void ALSADevice::setVoipMicMute(int state)
{
    LOGD("setVoipMicMute: state %d", state);
    setMixerControl("Voip Tx Mute", state, 0);
}

void ALSADevice::setBtscoRate(int rate)
{
    btsco_samplerate = rate;
}

void ALSADevice::enableWideVoice(bool flag)
{
    LOGD("enableWideVoice: flag %d", flag);
    if(flag == true) {
        setMixerControl("Widevoice Enable", 1, 0);
    } else {
        setMixerControl("Widevoice Enable", 0, 0);
    }
}

void ALSADevice::enableFENS(bool flag)
{
    LOGD("enableFENS: flag %d", flag);
    if(flag == true) {
        setMixerControl("FENS Enable", 1, 0);
    } else {
        setMixerControl("FENS Enable", 0, 0);
    }
}

void ALSADevice::setFlags(uint32_t flags)
{
    LOGV("setFlags: flags %d", flags);
    mDevSettingsFlag = flags;
}

status_t ALSADevice::getMixerControl(const char *name, unsigned int &value, int index)
{
    struct mixer_ctl *ctl;

    if (!mMixer) {
        LOGE("Control not initialized");
        return NO_INIT;
    }

    ctl =  mixer_get_control(mMixer, name, index);
    if (!ctl)
        return BAD_VALUE;

    mixer_ctl_get(ctl, &value);
    return NO_ERROR;
}

status_t ALSADevice::setMixerControl(const char *name, unsigned int value, int index)
{
    struct mixer_ctl *ctl;
    int ret = 0;
    LOGD("set:: name %s value %d index %d", name, value, index);
    if (!mMixer) {
        LOGE("Control not initialized");
        return NO_INIT;
    }

    // ToDo: Do we need to send index here? Right now it works with 0
    ctl = mixer_get_control(mMixer, name, 0);
    if(ctl == NULL) {
        LOGE("Could not get the mixer control");
        return BAD_VALUE;
    }
    ret = mixer_ctl_set(ctl, value);
    return (ret < 0) ? BAD_VALUE : NO_ERROR;
}

status_t ALSADevice::setMixerControl(const char *name, const char *value)
{
    struct mixer_ctl *ctl;
    int ret = 0;
    LOGD("set:: name %s value %s", name, value);

    if (!mMixer) {
        LOGE("Control not initialized");
        return NO_INIT;
    }

    ctl = mixer_get_control(mMixer, name, 0);
    if(ctl == NULL) {
        LOGE("Could not get the mixer control");
        return BAD_VALUE;
    }
    ret = mixer_ctl_select(ctl, value);
    return (ret < 0) ? BAD_VALUE : NO_ERROR;
}

int32_t ALSADevice::get_linearpcm_channel_status(uint32_t sampleRate,
                                                 unsigned char *channel_status)
{
    int32_t status = 0;
    unsigned char bit_index;
    memset(channel_status,0,24);
    bit_index = 0;
    /* block start bit in preamble bit 3 */
    set_bits(channel_status, 1, 1, &bit_index);

    //linear pcm
    bit_index = 1;
    set_bits(channel_status, 1, 0, &bit_index);

    bit_index = 24;
    switch (sampleRate) {
        case 8000:
           set_bits(channel_status, 4, 0x09, &bit_index);
           break;
        case 11025:
           set_bits(channel_status, 4, 0x0A, &bit_index);
           break;
        case 12000:
           set_bits(channel_status, 4, 0x0B, &bit_index);
           break;
        case 16000:
           set_bits(channel_status, 4, 0x0E, &bit_index);
           break;
        case 22050:
           set_bits(channel_status, 4, 0x02, &bit_index);
           break;
        case 24000:
           set_bits(channel_status, 4, 0x06, &bit_index);
           break;
        case 32000: // 1100 in 24..27
           set_bits(channel_status, 4, 0x0C, &bit_index);
           break;
        case 44100: // 0000 in 24..27
           break;
        case 48000: // 0100 in 24..27
            set_bits(channel_status, 4, 0x04, &bit_index);
            break;
        case 88200: // 0001 in 24..27
           set_bits(channel_status, 4, 0x01, &bit_index);
           break;
        case 96000: // 0101 in 24..27
            set_bits(channel_status, 4, 0x05, &bit_index);
            break;
        case 176400: // 0011 in 24..27
            set_bits(channel_status, 4, 0x03, &bit_index);
            break;
        case 192000: // 0111 in 24..27
            set_bits(channel_status, 4, 0x07, &bit_index);
            break;
        default:
            LOGV("Invalid sample_rate %u\n", sampleRate);
            status = -1;
            break;
    }
    return status;
}

int32_t ALSADevice::get_compressed_channel_status(void *audio_stream_data,
                                                  uint32_t audio_frame_size,
                                                  unsigned char *channel_status,
                                                  enum audio_parser_code_type codec_type)
                                                  // codec_type - AUDIO_PARSER_CODEC_AC3
                                                  //            - AUDIO_PARSER_CODEC_DTS
{
    unsigned char *streamPtr;
    streamPtr = (unsigned char *)audio_stream_data;

    if(init_audio_parser(streamPtr, audio_frame_size, codec_type) == -1)
    {
        LOGE("init audio parser failed");
        return -1;
    }
    get_channel_status(channel_status, codec_type);
    return 0;
}

status_t ALSADevice::setPcmVolume(int value)
{
    status_t err = NO_ERROR;

    err = setMixerControl("HIFI2 RX Volume",value,0);
    if(err) {
        LOGE("setPcmVolume - HIFI2 error = %d",err);
    }

    return err;
}

status_t ALSADevice::setCompressedVolume(int value)
{
    status_t err = NO_ERROR;

    err = setMixerControl("COMPRESSED RX Volume",value,0);
    if(err) {
        LOGE("setCompressedVolume = error = %d",err);
    }

    return err;
}

status_t ALSADevice::setPlaybackFormat(const char *value)
{
    status_t err = NO_ERROR;

    err = setMixerControl("SEC RX Format",value);
    if(err) {
        LOGE("setPlaybackFormat error = %d",err);
    }

    return err;
}

status_t ALSADevice::setWMAParams(alsa_handle_t *handle, int params[], int size)
{
    status_t err = NO_ERROR;
    if (size > sizeof(mWMA_params)/sizeof(mWMA_params[0])) {
        LOGE("setWMAParams too many params error");
        return BAD_VALUE;
    }
    for (int i = 0; i < size; i++)
        mWMA_params[i] = params[i];
    return err;
}

int ALSADevice::getALSABufferSize(alsa_handle_t *handle) {

    int format = 2;

    switch (handle->format) {
        case SNDRV_PCM_FORMAT_S8:
            format = 1;
        break;
        case SNDRV_PCM_FORMAT_S16_LE:
            format = 2;
        break;
        case SNDRV_PCM_FORMAT_S24_LE:
            format = 3;
        break;
        default:
           format = 2;
        break;
    }
    LOGD("getALSABufferSize - handle->channels = %d,  handle->sampleRate = %d,\
            format = %d",handle->channels, handle->sampleRate,format);

    LOGD("buff size is %d",((PCM_BUFFER_DURATION *  handle->channels\
            *  handle->sampleRate * format) / 1000000));
    int bufferSize = ((PCM_BUFFER_DURATION *  handle->channels
            *  handle->sampleRate * format) / 1000000);


    //Check for power of 2
    if (bufferSize & (bufferSize-1)) {

        bufferSize -= 1;
        for (uint32_t i=1; i<sizeof(bufferSize -1)*CHAR_BIT; i<<=1)
                bufferSize = bufferSize | bufferSize >> i;
        bufferSize += 1;
        LOGV("Not power of 2 - buff size is = %d",bufferSize);
    }
    else {
        LOGV("power of 2");
    }
    if(bufferSize < MULTI_CHANNEL_MIN_PERIOD_SIZE)
        bufferSize = MULTI_CHANNEL_MIN_PERIOD_SIZE;
    if(bufferSize >  MULTI_CHANNEL_MAX_PERIOD_SIZE)
        bufferSize = MULTI_CHANNEL_MAX_PERIOD_SIZE;

    return bufferSize;
}

status_t ALSADevice::setHDMIChannelCount()
{
    status_t err = NO_ERROR;

    err = setMixerControl("HDMI_RX Channels","Two");
    if(err) {
        LOGE("setHDMIChannelCount error = %d",err);
    }
    return err;
}

}
