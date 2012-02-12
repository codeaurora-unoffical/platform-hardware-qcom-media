/* AudioBroadcastStreamALSA.cpp
 **
 ** Copyright 2008-2009 Wind River Systems
 ** Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <math.h>

#define LOG_TAG "AudioBroadcastStreamALSA"
//#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include <linux/ioctl.h>

#include "AudioHardwareALSA.h"

namespace android_audio_legacy
{

// ----------------------------------------------------------------------------

AudioBroadcastStreamALSA::AudioBroadcastStreamALSA(AudioHardwareALSA *parent,
                                                   uint32_t  devices,
                                                   int      *format,
                                                   uint32_t *channels,
                                                   uint32_t *sampleRate,
                                                   uint32_t audioSource,
                                                   status_t *status)
{
    mParent           = parent;
    mFrameCount       = 0;
    mPcmRxHandle      = NULL;
    mPcmTxHandle      = NULL;
    mComprRxHandle    = NULL;
    mComprTxHandle    = NULL;
    mSpdifHandle      = NULL;

    *status           = BAD_VALUE;

    mRouteCompressedAudio = false;
    mRoutePcmStereo       = false;
    mSetupDSPLoopback     = false;
    mPullAudioFromDSP     = false;
    mOpenMS11Decoder      = false;

    // Check if audio is to be routed to SPDIF or not
    if(devices & AudioSystem::DEVICE_OUT_SPDIF) {
        mRouteAudioToSpdif = true;
    } else {
        mRouteAudioToSpdif = false;
    }
    if(devices & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE || devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET
       || devices & AudioSystem::DEVICE_OUT_SPEAKER) {
        mRoutePcmStereo = true;
    }
    // ToDo: What about HDMI output device ??
    
    // 1. Validate the audio source type
    if(audioSource == QCOM_AUDIO_SOURCE_ANALOG_BROADCAST) {
        // If the audio source is Analog broadcast the audio could be in PCM mono/stereo
        // there is no need for MS11 decoder, HAL just needs to initiate DSP loop back
        mSetupDSPLoopback = true;
    } else if(audioSource == QCOM_AUDIO_SOURCE_HDMI_IN) {
        if( *format == AudioSystem::PCM_16_BIT) {
            // If the audio source is HDMI input and channels are MONO/STEREO or output device is HDMI
            // there is no need for MS11 decoder, HAL just needs to initiate DSP loop back
            if(*channels == 1 || *channels == 2 || devices == AudioSystem::DEVICE_OUT_AUX_DIGITAL){
                // Handle DSP loop back here
                mSetupDSPLoopback = true;
            } else {
                // Pull audio data from DSP
                // If the data is 5.1, encode it to AC3 using MS11 decoder and open two streams to write
                // downmixed 2.0 PCM and compressed AC3 data to SPDIF
                mPullAudioFromDSP = true;
                mOpenMS11Decoder  = true;
            }
        } else {
            // Pull the compressed data from DSP
            mPullAudioFromDSP = true;
            if(*format == AudioSystem::AC3 || *format == AudioSystem::AC3_PLUS ||
               *format == AudioSystem::AAC || *format == AudioSystem::HE_AAC_V1 ||
               *format == AudioSystem::HE_AAC_V2) {
                // If the format is AC3/AAC decode it using MS11 decoder
                mOpenMS11Decoder  = true;
            } else {
                // If the format is non AC3/AAC write it back to DSP
                mPullAudioFromDSP = true;
                mRouteCompressedAudio = true;
            }
        }
    } else if(audioSource == QCOM_AUDIO_SOURCE_DIGITAL_BROADCAST_MAIN_ONLY ||
              audioSource == QCOM_AUDIO_SOURCE_DIGITAL_BROADCAST_MAIN_AD) {
        if(*format == AudioSystem::AC3 || *format == AudioSystem::AC3_PLUS ||
           *format == AudioSystem::AAC || *format == AudioSystem::HE_AAC_V1 ||
           *format == AudioSystem::HE_AAC_V2) {
            // If the format is AC3/AAC decode it using MS11 decoder
            mOpenMS11Decoder  = true;
        } else {
            // If the format is non AC3/AAC write it to DSP
            mRouteCompressedAudio = true;
            // ToDo: What about mixing main and AD in this case?
            // Should we pull back both the main and AD decoded data and mix using
            // MS11 decoder?
        }
    } else {
        LOGE("Invalid audio source type");
        return;
    }

    // 1. Check the device is if Speaker/Headset/Headphone and if found 
    //    set HiFi usecase/modifier and open the pcm device

    // 3. Instantiate the Dolby MS11 decoder instance and configure it
    char *use_case;
    alsa_handle_t alsa_handle;
    if(mPullAudioFromDSP) {
        // ToDo: Open the capture driver and route audio
        if(*format == AudioSystem::PCM_16_BIT) {
            // Open PCM capture driver
            bool bIsUseCaseSet = false;
            alsa_handle.module = mParent->mALSADevice;
            alsa_handle.bufferSize = DEFAULT_BUFFER_SIZE;
            alsa_handle.devices = devices;
            alsa_handle.handle = 0;
            alsa_handle.format = SNDRV_PCM_FORMAT_S16_LE;
            alsa_handle.channels = DEFAULT_CHANNEL_MODE; // ToDo: Change this accordingly
            alsa_handle.sampleRate = DEFAULT_SAMPLING_RATE;
            alsa_handle.latency = RECORD_LATENCY;
            alsa_handle.rxHandle = 0;
            alsa_handle.ucMgr = mParent->mUcMgr;
            snd_use_case_get(mParent->mUcMgr, "_verb", (const char **)&use_case);
            if ((use_case == NULL) || (!strcmp(use_case, SND_USE_CASE_VERB_INACTIVE))) {
                strlcpy(alsa_handle.useCase, SND_USE_CASE_VERB_HIFI_REC, sizeof(alsa_handle.useCase));
                bIsUseCaseSet = true;
            } else {
                strlcpy(alsa_handle.useCase, SND_USE_CASE_MOD_CAPTURE_MUSIC, sizeof(alsa_handle.useCase));
            }
            free(use_case);
            mParent->mDeviceList.push_back(alsa_handle);
            ALSAHandleList::iterator it = mParent->mDeviceList.end(); it--;
            LOGD("useCase %s", it->useCase);
            mParent->mALSADevice->route(&(*it), devices, mParent->mode());
            mPcmRxHandle = &(*it);
            if(bIsUseCaseSet) {
                snd_use_case_set(mParent->mUcMgr, "_verb", it->useCase);
            } else {
                snd_use_case_set(mParent->mUcMgr, "_enamod", it->useCase);
            }
            *status = mParent->mALSADevice->open(&(*it));
        } else {
            // Open Compressed capture driver
            bool bIsUseCaseSet = false;
            alsa_handle.module = mParent->mALSADevice;
            alsa_handle.bufferSize = DEFAULT_BUFFER_SIZE;
            alsa_handle.devices = devices;
            alsa_handle.handle = 0;
            alsa_handle.format = SNDRV_PCM_FORMAT_S16_LE;
            alsa_handle.channels = DEFAULT_CHANNEL_MODE; // ToDo: Change this accordingly
            alsa_handle.sampleRate = DEFAULT_SAMPLING_RATE;
            alsa_handle.latency = RECORD_LATENCY;
            alsa_handle.rxHandle = 0;
            alsa_handle.ucMgr = mParent->mUcMgr;
#if 0
            snd_use_case_get(mParent->mUcMgr, "_verb", (const char **)&use_case);
            if ((use_case == NULL) || (!strcmp(use_case, SND_USE_CASE_VERB_INACTIVE))) {
                strlcpy(alsa_handle.useCase, SND_USE_CASE_VERB_HIFI_REC_COMPRESSED, sizeof(alsa_handle.useCase));
                bIsUseCaseSet = true;
            } else {
                strlcpy(alsa_handle.useCase, SND_USE_CASE_MOD_CAPTURE_MUSIC_COMPRESSED, sizeof(alsa_handle.useCase));
            }
#endif
            free(use_case);
            mParent->mDeviceList.push_back(alsa_handle);
            ALSAHandleList::iterator it = mParent->mDeviceList.end(); it--;
            LOGD("useCase %s", it->useCase);
            mParent->mALSADevice->route(&(*it), devices, mParent->mode());
            mPcmRxHandle = &(*it);
            if(bIsUseCaseSet) {
                snd_use_case_set(mParent->mUcMgr, "_verb", it->useCase);
            } else {
                snd_use_case_set(mParent->mUcMgr, "_enamod", it->useCase);
            }
            *status = mParent->mALSADevice->open(&(*it));
        }
    }
    if(mRoutePcmStereo && !mSetupDSPLoopback) {
        // Create the HiFi2 use case and HiFi3 if HiFi2 is already in use.
        // The below code is to test with exusting HiFi use case on hw0,0 driver
        // ToDo: Once the UCM changes are available, change this to use HiFi2/3
        bool bIsUseCaseSet = false;
        alsa_handle.module = mParent->mALSADevice;
        alsa_handle.bufferSize = DEFAULT_BUFFER_SIZE;
        alsa_handle.devices = devices;
        alsa_handle.handle = 0;
        alsa_handle.format = SNDRV_PCM_FORMAT_S16_LE;
        alsa_handle.channels = DEFAULT_CHANNEL_MODE;
        alsa_handle.sampleRate = DEFAULT_SAMPLING_RATE;
        alsa_handle.latency = PLAYBACK_LATENCY;
        alsa_handle.rxHandle = 0;
        alsa_handle.ucMgr = mParent->mUcMgr;
        snd_use_case_get(mParent->mUcMgr, "_verb", (const char **)&use_case);
        if ((use_case == NULL) || (!strcmp(use_case, SND_USE_CASE_VERB_INACTIVE))) {
            strlcpy(alsa_handle.useCase, SND_USE_CASE_VERB_HIFI2, sizeof(alsa_handle.useCase));
        } else {
            strlcpy(alsa_handle.useCase, SND_USE_CASE_MOD_PLAY_MUSIC2, sizeof(alsa_handle.useCase));
        }
        free(use_case);
        mParent->mDeviceList.push_back(alsa_handle);
        ALSAHandleList::iterator it = mParent->mDeviceList.end(); it--;
        LOGD("useCase %s", it->useCase);
        mParent->mALSADevice->route(&(*it), devices, mParent->mode());
        mComprRxHandle = &(*it);
        if(bIsUseCaseSet) {
            snd_use_case_set(mParent->mUcMgr, "_verb", it->useCase);
        } else {
            snd_use_case_set(mParent->mUcMgr, "_enamod", it->useCase);
        }
        *status = mParent->mALSADevice->open(&(*it));
    }
    if((mRouteCompressedAudio || mRouteAudioToSpdif)&& !mSetupDSPLoopback) {
        // ToDo: Set up HiFiCompressed use case and set up compressed playback
        bool bIsUseCaseSet = false;
        alsa_handle.module = mParent->mALSADevice;
        alsa_handle.bufferSize = DEFAULT_BUFFER_SIZE;
        alsa_handle.devices = devices;
        alsa_handle.handle = 0;
        alsa_handle.format = SNDRV_PCM_FORMAT_S16_LE;
        alsa_handle.channels = DEFAULT_CHANNEL_MODE;
        alsa_handle.sampleRate = DEFAULT_SAMPLING_RATE;
        alsa_handle.latency = PLAYBACK_LATENCY;
        alsa_handle.rxHandle = 0;
        alsa_handle.ucMgr = mParent->mUcMgr;
#if 0
        snd_use_case_get(mParent->mUcMgr, "_verb", (const char **)&use_case);
        if ((use_case == NULL) || (!strcmp(use_case, SND_USE_CASE_VERB_INACTIVE))) {
            strlcpy(alsa_handle.useCase, SND_USE_CASE_VERB_HIFI_COMPRESSED, sizeof(alsa_handle.useCase));
            bIsUseCaseSet = true;
        } else {
            strlcpy(alsa_handle.useCase, SND_USE_CASE_MOD_PLAY_MUSIC_COMPRESSED, sizeof(alsa_handle.useCase));
        }
#endif
        free(use_case);
        mParent->mDeviceList.push_back(alsa_handle);
        ALSAHandleList::iterator it = mParent->mDeviceList.end(); it--;
        LOGD("useCase %s", it->useCase);
        mParent->mALSADevice->route(&(*it), devices, mParent->mode());
        mPcmRxHandle = &(*it);
        if(bIsUseCaseSet) {
            snd_use_case_set(mParent->mUcMgr, "_verb", it->useCase);
        } else {
            snd_use_case_set(mParent->mUcMgr, "_enamod", it->useCase);
        }
        *status = mParent->mALSADevice->open(&(*it));
    }
    if(mSetupDSPLoopback) {
        // ToDo: Set up for audio loop back in DSP itself
    }
}

AudioBroadcastStreamALSA::~AudioBroadcastStreamALSA()
{
}

status_t AudioBroadcastStreamALSA::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 key = String8(AudioParameter::keyRouting);
    int device;
    if (param.getInt(key, device) == NO_ERROR) {
        // Ignore routing if device is 0.
        LOGD("setParameters(): keyRouting with device %d", device);
        mDevices = device;
        if(device) {
            //ToDo: Call device setting UCM API here
        }
        param.remove(key);
    }

    return NO_ERROR;
}

String8 AudioBroadcastStreamALSA::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {
        param.addInt(key, (int)mDevices);
    }

    LOGV("getParameters() %s", param.toString().string());
    return param.toString();
}

status_t AudioBroadcastStreamALSA::start(int64_t absTimeToStart)
{
    status_t err = OK;
    // 1. Set the absolute time stamp
    // ToDo: We need the ioctl from driver to set the time stamp
    
    // 2. Signal the driver to start rendering data
    if (ioctl(mPcmRxHandle->handle->fd, SNDRV_PCM_IOCTL_START)) {
        LOGE("start:SNDRV_PCM_IOCTL_START failed\n");
        err = UNKNOWN_ERROR;
    }
    return err;
}

status_t AudioBroadcastStreamALSA::mute(bool mute)
{
    if(mute) {
        // Set the volume to 0 to mute the stream
        //mPcmRxHandle->module->setVolume(mStreamVol);
    } else {
        // Set the volume back to current volume
        //mPcmRxHandle->module->setVolume(mStreamVol);
    }
    return OK;
}

status_t AudioBroadcastStreamALSA::setVolume(float left, float right)
{
    float volume;
    status_t status = NO_ERROR;

    LOGE("ToDo: setVolume:: Add correct condition check for the appropriate use case/modifier");
    if(mPcmRxHandle && (!strcmp(mPcmRxHandle->useCase, SND_USE_CASE_VERB_HIFI) ||
                        !strcmp(mPcmRxHandle->useCase, SND_USE_CASE_MOD_PLAY_MUSIC))) {
        volume = (left + right) / 2;
        if (volume < 0.0) {
            LOGW("AudioBroadcastStreamALSA::setVolume(%f) under 0.0, assuming 0.0\n", volume);
            volume = 0.0;
        } else if (volume > 1.0) {
            LOGW("AudioBroadcastStreamALSA::setVolume(%f) over 1.0, assuming 1.0\n", volume);
            volume = 1.0;
        }
        mStreamVol = lrint((volume * 100.0)+0.5);

        LOGD("Setting broadcast stream volume to %d (available range is 0 to 100)\n", mStreamVol);
        LOGE("ToDo: Implement volume setting for broadcast stream");
        //mPcmRxHandle->module->setVolume(mStreamVol);

        return status;
    }
    return INVALID_OPERATION;
}

ssize_t AudioBroadcastStreamALSA::write(const void *buffer, size_t bytes, 
                                        int64_t timestamp, int audiotype)
{
    int period_size;
    char *use_case;

    LOGV("write:: buffer %p, bytes %d", buffer, bytes);
    if (!mPowerLock) {
        acquire_wake_lock (PARTIAL_WAKE_LOCK, "AudioOutLock");
        mPowerLock = true;
    }

    snd_pcm_sframes_t n;
    size_t            sent = 0;
    status_t          err;

    // 1. Check if MS11 decoder instance is present and if present we need to
    //    preserve the data and supply it to MS 11 decoder.
    if(mMS11Decoder != NULL) {
    }

    // 2. Get the output data from MS11 decoder and write to PCM driver
    if(mPcmRxHandle && mRoutePcmStereo) {
        int write_pending = bytes;
        period_size = mPcmRxHandle->periodSize;
        do {
            if (write_pending < period_size) {
                LOGE("write:: We should not be here !!!");
                write_pending = period_size;
            }
            n = pcm_write(mPcmRxHandle->handle,
                     (char *)buffer + sent,
                      period_size);
            if (n == -EBADFD) {
                // Somehow the stream is in a bad state. The driver probably
                // has a bug and snd_pcm_recover() doesn't seem to handle this.
                mPcmRxHandle->module->open(mPcmRxHandle);
            }
            else if (n < 0) {
                // Recovery is part of pcm_write. TODO split is later.
                LOGE("pcm_write returned n < 0");
                return static_cast<ssize_t>(n);
            }
            else {
                mFrameCount += n;
                sent += static_cast<ssize_t>((period_size));
                write_pending -= period_size;
            }
        } while ((mPcmRxHandle->handle) && (sent < bytes));
    }

    return bytes;
}

status_t AudioBroadcastStreamALSA::dump(int fd, const Vector<String16>& args)
{
    return NO_ERROR;
}

status_t AudioBroadcastStreamALSA::standby()
{
    LOGD("standby");
    if(mPcmRxHandle) {
        mPcmRxHandle->module->standby(mPcmRxHandle);
    }

    if (mPowerLock) {
        release_wake_lock ("AudioBroadcastLock");
        mPowerLock = false;
    }

    mFrameCount = 0;

    return NO_ERROR;
}

#define USEC_TO_MSEC(x) ((x + 999) / 1000)

uint32_t AudioBroadcastStreamALSA::latency() const
{
    // Android wants latency in milliseconds.
    return USEC_TO_MSEC (mPcmRxHandle->latency);
}

// return the number of audio frames written by the audio dsp to DAC since
// the output has exited standby
status_t AudioBroadcastStreamALSA::getRenderPosition(uint32_t *dspFrames)
{
    *dspFrames = mFrameCount;
    return NO_ERROR;
}

}       // namespace android_audio_legacy
