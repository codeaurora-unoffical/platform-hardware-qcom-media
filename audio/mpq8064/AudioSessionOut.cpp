/* AudioSessionOutALSA.cpp
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

#define LOG_TAG "AudioSessionOutALSA"
#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include <linux/ioctl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <pthread.h>

#include "AudioHardwareALSA.h"

namespace android_audio_legacy
{

// ----------------------------------------------------------------------------

AudioSessionOutALSA::AudioSessionOutALSA(AudioHardwareALSA *parent,
                                         uint32_t   devices,
                                         int        format,
                                         uint32_t   channels,
                                         uint32_t   samplingRate,
                                         int        sessionId,
                                         status_t   *status)
{
    alsa_handle_t alsa_handle;
    char *use_case;
    bool bIsUseCaseSet = false;

    Mutex::Autolock autoLock(mLock);
    if(devices == 0) {
        LOGE("No output device specified");
        return;
    }
    if((format == AUDIO_FORMAT_PCM_16_BIT) && (channels == 0 || channels > 6)) {
        LOGE("Invalid number of channels %d", channels);
        return;
    }

    // Default initilization
    mParent         = parent;
    mALSADevice     = mParent->mALSADevice;
    mUcMgr          = mParent->mUcMgr;
    mFrameCount     = 0;
    mFormat         = format;
    mSampleRate     = samplingRate;
    mChannels       = channels;
    mDevices        = devices;
    mBufferSize     = 0;
    *status         = BAD_VALUE;

    mUseTunnelDecode    = false;
    mCaptureFromProxy   = false;
    mRoutePcmAudio      = false;
    mRoutePcmToSpdif    = false;
    mRouteCompreToSpdif = false;
    mRouteAudioToA2dp   = false;
    mA2dpOutputStarted  = false;

    mPcmRxHandle        = NULL;
    mPcmTxHandle        = NULL;
    mSpdifRxHandle      = NULL;
    mCompreRxHandle     = NULL;
    mProxyPcmHandle     = NULL;

    if(mDevices & AudioSystem::DEVICE_OUT_ALL_A2DP) {
        mCaptureFromProxy = true;
        mRouteAudioToA2dp = true;
        mDevices &= ~AudioSystem::DEVICE_OUT_ALL_A2DP;
        // ToDo: Handle A2DP+Speaker
        //mDevices |= AudioSystem::DEVICE_OUT_PROXY;
        mDevices = AudioSystem::DEVICE_OUT_PROXY;
    }
    if(format == AUDIO_FORMAT_AAC || format == AUDIO_FORMAT_HE_AAC_V1 ||
       format == AUDIO_FORMAT_HE_AAC_V2 || format == AUDIO_FORMAT_AC3 ||
       format == AUDIO_FORMAT_AC3_PLUS) {
        // Instantiate MS11 decoder for single decode use case
        if(mDevices & ~AudioSystem::DEVICE_OUT_SPDIF) {
            mRoutePcmAudio = true;
        }
        if(devices & AudioSystem::DEVICE_OUT_SPDIF) {
            mRouteCompreToSpdif = true;
        }
    } else if(format == AUDIO_FORMAT_WMA || format == AUDIO_FORMAT_DTS ||
              format == AUDIO_FORMAT_MP3){
        // In this case, DSP will decode and route the PCM data to output devices
        mUseTunnelDecode = true;
        if(devices & AudioSystem::DEVICE_OUT_PROXY) {
            mCaptureFromProxy = true;
        }
    } else if(format == AUDIO_FORMAT_PCM_16_BIT) {
        if(mDevices & ~AudioSystem::DEVICE_OUT_SPDIF) {
            mRoutePcmAudio = true;
        }
        if(channels > 2 && channels <= 6) {
            // Instantiate MS11 decoder for downmix and re-encode
            if(devices & AudioSystem::DEVICE_OUT_SPDIF) {
                mRouteCompreToSpdif = true;
            }
        } else {
            if(devices & AudioSystem::DEVICE_OUT_SPDIF) {
                mRoutePcmToSpdif = true;
            }
        }
    } else {
        LOGE("Unsupported format %d", format);
        return;
    }

    if(mRoutePcmAudio) {
        // If the same audio PCM is to be routed to SPDIF also, do not remove from 
        // device list
        if(!mRoutePcmToSpdif) {
            devices = mDevices & ~AudioSystem::DEVICE_OUT_SPDIF;
        }
        snd_use_case_get(mUcMgr, "_verb", (const char **)&use_case);
        if ((use_case == NULL) || (!strcmp(use_case, SND_USE_CASE_VERB_INACTIVE))) {
            *status = openDevice(SND_USE_CASE_VERB_HIFI2, true, devices);
        } else {
            *status = openDevice(SND_USE_CASE_MOD_PLAY_MUSIC2, false, devices);
        }
        free(use_case);
        if(*status != NO_ERROR) {
            return;
        }
        ALSAHandleList::iterator it = mParent->mDeviceList.end(); it--;
        mPcmRxHandle = &(*it);
        mBufferSize = mPcmRxHandle->periodSize;
    }
    if(mUseTunnelDecode) {
        // If audio is to be capture back from proxy device, then route 
        // audio to SPDIF and Proxy devices only
        devices = mDevices;
        if(mCaptureFromProxy) {
            devices = (mDevices & AudioSystem::DEVICE_OUT_SPDIF);
            devices |= AudioSystem::DEVICE_OUT_PROXY;
        }
#if 0
        snd_use_case_get(mUcMgr, "_verb", (const char **)&use_case);
        if ((use_case == NULL) || (!strcmp(use_case, SND_USE_CASE_VERB_INACTIVE))) {
            *status = openDevice(SND_USE_CASE_VERB_HIFI_TUNNEL, true, devices);
        } else {
        *status = openDevice(SND_USE_CASE_MOD_PLAY_MUSIC_TUNNEL, false, devices);
        }
        free(use_case);
#endif
        if(*status != NO_ERROR) {
            return;
        }
        ALSAHandleList::iterator it = mParent->mDeviceList.end(); it--;
        mCompreRxHandle = &(*it);
        mBufferSize = mCompreRxHandle->periodSize;
    } else if (mRouteCompreToSpdif) {
        devices = AudioSystem::DEVICE_OUT_SPDIF;
        if(mCaptureFromProxy) {
            devices |= AudioSystem::DEVICE_OUT_PROXY;
        }
#if 0
        snd_use_case_get(mUcMgr, "_verb", (const char **)&use_case);
        if ((use_case == NULL) || (!strcmp(use_case, SND_USE_CASE_VERB_INACTIVE))) {
            *status = openDevice(SND_USE_CASE_VERB_HIFI_COMPRESSED, true, devices);
        } else {
        *status = openDevice(SND_USE_CASE_MOD_PLAY_MUSIC_COMPRESSED, false, devices);
        }
        free(use_case);
#endif
        if(*status != NO_ERROR) {
            return;
        }
        ALSAHandleList::iterator it = mParent->mDeviceList.end(); it--;
        mCompreRxHandle = &(*it);
    }
    if(mCaptureFromProxy) {
        status_t err = openProxyDevice();
        if(!err && mRouteAudioToA2dp) {
            err = openA2dpOutput();
        }
        *status = err;
    }
}

AudioSessionOutALSA::~AudioSessionOutALSA()
{
    Mutex::Autolock autoLock(mLock);
    LOGV("~AudioSessionOutALSA");
    if(mProxyPcmHandle) {
        LOGV("Closing the Proxy device: mProxyPcmHandle %p", mProxyPcmHandle);
        pcm_close(mProxyPcmHandle);
        mProxyPcmHandle = NULL;
    }
    if(mA2dpOutputStarted) {
        stopA2dpOutput();
        closeA2dpOutput();
    }
}

status_t AudioSessionOutALSA::setParameters(const String8& keyValuePairs)
{
    Mutex::Autolock autoLock(mLock);
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 key = String8(AudioParameter::keyRouting);
    int device;
    if (param.getInt(key, device) == NO_ERROR) {
        // Ignore routing if device is 0.
        LOGD("setParameters(): keyRouting with device %d", device);
        mDevices = device;
        if(device) {
            //ToDo: Call device setting UCM API here
            doRouting(device);
        }
        param.remove(key);
    }

    return NO_ERROR;
}

String8 AudioSessionOutALSA::getParameters(const String8& keys)
{
    Mutex::Autolock autoLock(mLock);
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {
        param.addInt(key, (int)mDevices);
    }

    LOGV("getParameters() %s", param.toString().string());
    return param.toString();
}

status_t AudioSessionOutALSA::setVolume(float left, float right)
{
    Mutex::Autolock autoLock(mLock);
    float volume;
    status_t status = NO_ERROR;

    if(mPcmRxHandle) {
        volume = (left + right) / 2;
        if (volume < 0.0) {
            LOGW("AudioSessionOutALSA::setVolume(%f) under 0.0, assuming 0.0\n", volume);
            volume = 0.0;
        } else if (volume > 1.0) {
            LOGW("AudioSessionOutALSA::setVolume(%f) over 1.0, assuming 1.0\n", volume);
            volume = 1.0;
        }
        mStreamVol = lrint((volume * 100.0)+0.5);

        LOGD("Setting stream volume to %d (available range is 0 to 100)\n", mStreamVol);
        LOGE("ToDo: Implement volume setting for broadcast stream");
        //mALSADevice->setStreamVolume(mPcmRxHandle, mStreamVol);

        return status;
    }
    return INVALID_OPERATION;
}

ssize_t AudioSessionOutALSA::write(const void *buffer, size_t bytes)
{
    int period_size;
    char *use_case;

    Mutex::Autolock autoLock(mLock);
    LOGV("write:: buffer %p, bytes %d", buffer, bytes);
    if (!mPowerLock) {
        acquire_wake_lock (PARTIAL_WAKE_LOCK, "AudioSessionOutLock");
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
    if(mPcmRxHandle && mRoutePcmAudio) {
        int write_pending = bytes;
        period_size = mPcmRxHandle->periodSize;
        do {
            if (write_pending < period_size) {
                LOGE("write:: We should not be here !!!");
                write_pending = period_size;
            }
            LOGE("Calling pcm_write");
            n = pcm_write(mPcmRxHandle->handle,
                     (char *)buffer + sent,
                      period_size);
            LOGE("pcm_write returned with %d", n);
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

        if(mRouteAudioToA2dp && !mA2dpOutputStarted) {
            startA2dpOutput();
            mA2dpOutputStarted = true;
        }
    }

    return bytes;
}

status_t AudioSessionOutALSA::start(int64_t startTime)
{
    Mutex::Autolock autoLock(mLock);
    status_t err = NO_ERROR;
    // 1. Set the absolute time stamp
    // ToDo: We need the ioctl from driver to set the time stamp

    // 2. Signal the driver to start rendering data
    if (ioctl(mPcmRxHandle->handle->fd, SNDRV_PCM_IOCTL_START)) {
        LOGE("start:SNDRV_PCM_IOCTL_START failed\n");
        err = UNKNOWN_ERROR;
    }
    return err;
}

status_t AudioSessionOutALSA::pause()
{
    Mutex::Autolock autoLock(mLock);
    if(mPcmRxHandle) {
        if (ioctl(mPcmRxHandle->handle->fd, SNDRV_PCM_IOCTL_PAUSE,1) < 0) {
            LOGE("RESUME failed for use case %s", mPcmRxHandle->useCase);
        }
    }
    if(mCompreRxHandle) {
        if (ioctl(mCompreRxHandle->handle->fd, SNDRV_PCM_IOCTL_PAUSE,1) < 0) {
            LOGE("RESUME failed on use case %s", mCompreRxHandle->useCase);
        }
    }
    return NO_ERROR;
}

status_t AudioSessionOutALSA::flush()
{
    Mutex::Autolock autoLock(mLock);
    LOGD("AudioSessionOutALSA::flush E");
    if(mPcmRxHandle) {
        if (ioctl(mPcmRxHandle->handle->fd, SNDRV_PCM_IOCTL_PAUSE,1) < 0) {
            LOGE("flush(): Audio Pause failed");
        }
        mPcmRxHandle->handle->start = 0;
        pcm_prepare(mPcmRxHandle->handle);
        LOGV("flush(): Reset, drain and prepare completed");
        mPcmRxHandle->handle->sync_ptr->flags = (SNDRV_PCM_SYNC_PTR_APPL | 
                                                 SNDRV_PCM_SYNC_PTR_AVAIL_MIN);
        sync_ptr(mPcmRxHandle->handle);
    }
    LOGD("AudioSessionOutALSA::flush X");
    return NO_ERROR;
}

status_t AudioSessionOutALSA::resume()
{
    Mutex::Autolock autoLock(mLock);
    if(mPcmRxHandle) {
        if (ioctl(mPcmRxHandle->handle->fd, SNDRV_PCM_IOCTL_PAUSE,0) < 0) {
            LOGE("RESUME failed for use case %s", mPcmRxHandle->useCase);
        }
    }
    if(mCompreRxHandle) {
        if (ioctl(mCompreRxHandle->handle->fd, SNDRV_PCM_IOCTL_PAUSE,0) < 0) {
            LOGE("RESUME failed on use case %s", mCompreRxHandle->useCase);
        }
    }
    return NO_ERROR;
}

status_t AudioSessionOutALSA::stop()
{
    Mutex::Autolock autoLock(mLock);
    // close all the existing PCM devices
    // ToDo: How to make sure all the data is rendered before closing
    if(mPcmRxHandle)
        closeDevice(mPcmRxHandle);
    if(mCompreRxHandle)
        closeDevice(mCompreRxHandle);
    if(mPcmTxHandle)
        closeDevice(mPcmTxHandle);
    if(mSpdifRxHandle)
        closeDevice(mSpdifRxHandle);

    mRoutePcmAudio = false;
    mRouteCompreAudio = false;
    mRoutePcmToSpdif = false;
    mRouteCompreToSpdif = false;
    mUseTunnelDecode = false;
    mCaptureFromProxy = false;
    
    return NO_ERROR;
}

status_t AudioSessionOutALSA::dump(int fd, const Vector<String16>& args)
{
    return NO_ERROR;
}

status_t AudioSessionOutALSA::standby()
{
    Mutex::Autolock autoLock(mLock);
    LOGD("standby");
    if(mPcmRxHandle) {
        mPcmRxHandle->module->standby(mPcmRxHandle);
    }

    if (mPowerLock) {
        release_wake_lock ("AudioOutLock");
        mPowerLock = false;
    }

    mFrameCount = 0;

    return NO_ERROR;
}

#define USEC_TO_MSEC(x) ((x + 999) / 1000)

uint32_t AudioSessionOutALSA::latency() const
{
    // Android wants latency in milliseconds.
    return USEC_TO_MSEC (mPcmRxHandle->latency);
}

// return the number of audio frames written by the audio dsp to DAC since
// the output has exited standby
status_t AudioSessionOutALSA::getRenderPosition(uint32_t *dspFrames)
{
    Mutex::Autolock autoLock(mLock);
    *dspFrames = mFrameCount;
    return NO_ERROR;
}

status_t AudioSessionOutALSA::openA2dpOutput()
{
    hw_module_t *mod;
    int      format = AUDIO_FORMAT_PCM_16_BIT;
    uint32_t channels = AUDIO_CHANNEL_OUT_STEREO;
    uint32_t sampleRate = 44100;
    status_t status;
    LOGV("openA2dpOutput");
    int rc = hw_get_module_by_class(AUDIO_HARDWARE_MODULE_ID, (const char*)"a2dp", 
                                    (const hw_module_t**)&mod);
    if (rc) {
        LOGE("Could not get a2dp hardware module");
        return NO_INIT;
    }

    rc = audio_hw_device_open(mod, &mA2dpDevice);
    if(rc) {
        LOGE("couldn't open a2dp audio hw device");
        return NO_INIT;
    }
    status = mA2dpDevice->open_output_stream(mA2dpDevice, AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP, 
                                    &format, &channels, &sampleRate, &mA2dpStream);
    if(status != NO_ERROR) {
        LOGE("Failed to open output stream for a2dp: status %d", status);
    }
    return status;
}

status_t AudioSessionOutALSA::closeA2dpOutput()
{
    LOGV("closeA2dpOutput");
    if(!mA2dpDevice){
        LOGE("No Aactive A2dp output found");
        return BAD_VALUE;
    }

    mA2dpDevice->close_output_stream(mA2dpDevice, mA2dpStream);
    mA2dpStream = NULL;

    audio_hw_device_close(mA2dpDevice);
    mA2dpDevice = NULL;
    return NO_ERROR;
}

status_t AudioSessionOutALSA::startA2dpOutput()
{
    LOGV("startA2dpOutput");
    int err = pthread_create(&mA2dpThread, (const pthread_attr_t *) NULL,
                             a2dpThreadWrapper,
                             this);

    return err;
}

status_t AudioSessionOutALSA::stopA2dpOutput()
{
    LOGV("stopA2dpOutput");
    mExitA2dpThread = true;
    pthread_join(mA2dpThread,NULL);
    return NO_ERROR;
}

void *AudioSessionOutALSA::a2dpThreadWrapper(void *me) {
    static_cast<AudioSessionOutALSA *>(me)->a2dpThreadFunc();
    return NULL;
}

void AudioSessionOutALSA::a2dpThreadFunc()
{
    if(!mA2dpStream) {
        LOGE("No valid a2dp output stream found");
        return;
    }
    if(!mProxyPcmHandle) {
        LOGE("No valid mProxyPcmHandle found");
        return;
    }

    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);
    prctl(PR_SET_NAME, (unsigned long)"AudioHAL A2dpThread", 0, 0, 0);

    int a2dpBufSize = mA2dpStream->common.get_buffer_size(&mA2dpStream->common);

    void *a2dpBuffer = malloc(a2dpBufSize);
    if(!a2dpBuffer) {
        LOGE("Could not allocate buffer: a2dpBuffer");
        return;
    }
    int proxyBufSize = mProxyPcmHandle->period_size;
    void *proxyBuffer = malloc(proxyBufSize);
    if(!proxyBuffer) {
        LOGE("Could not allocate buffer: proxyBuffer");
        return;
    }

    while(!mExitA2dpThread) {
        // 1. Read from Proxy device
        int bytesRead = 0;
        while( (a2dpBufSize -  bytesRead) >= proxyBufSize) {
            int err = pcm_read(mProxyPcmHandle, a2dpBuffer + bytesRead, proxyBufSize);
            if(err) {
                LOGE("pcm_read on Proxy port failed with err %d", err);
            } else {
                LOGV("pcm_read on proxy device is success, bytesRead = %d", proxyBufSize);
            }
            bytesRead += proxyBufSize;
        }
        // 2. Buffer the data till the requested buffer size from a2dp output stream
        // 3. write to a2dp output stream
        LOGV("Writing %d bytes to a2dp output", bytesRead);
        mA2dpStream->write(mA2dpStream, a2dpBuffer, bytesRead);
    }
}

status_t AudioSessionOutALSA::openProxyDevice()
{
    char *deviceName = "hw:0,8";
    struct snd_pcm_hw_params *params = NULL;
    struct snd_pcm_sw_params *sparams = NULL;
    int flags = (PCM_IN | PCM_NMMAP | PCM_STEREO | DEBUG_ON);

    LOGV("openProxyDevice");
    mProxyPcmHandle = pcm_open(flags, deviceName);
    if (!pcm_ready(mProxyPcmHandle)) {
        LOGE("Opening proxy device failed");
        goto bail;
    }
    LOGV("Proxy device opened successfully: mProxyPcmHandle %p", mProxyPcmHandle);
    mProxyPcmHandle->channels = 2;
    mProxyPcmHandle->rate     = 48000;
    mProxyPcmHandle->flags    = flags;
    mProxyPcmHandle->period_size = 480;

    params = (struct snd_pcm_hw_params*) malloc(sizeof(struct snd_pcm_hw_params));
    if (!params) {
         goto bail;
    }

    param_init(params);

    param_set_mask(params, SNDRV_PCM_HW_PARAM_ACCESS,
                   (mProxyPcmHandle->flags & PCM_MMAP)? SNDRV_PCM_ACCESS_MMAP_INTERLEAVED
                   : SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
                   SNDRV_PCM_FORMAT_S16_LE);
    param_set_mask(params, SNDRV_PCM_HW_PARAM_SUBFORMAT,
                   SNDRV_PCM_SUBFORMAT_STD);
    param_set_min(params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, mProxyPcmHandle->period_size);
    param_set_int(params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 16);
    param_set_int(params, SNDRV_PCM_HW_PARAM_FRAME_BITS,
                   mProxyPcmHandle->channels - 1 ? 32 : 16);
    param_set_int(params, SNDRV_PCM_HW_PARAM_CHANNELS,
                   mProxyPcmHandle->channels);
    param_set_int(params, SNDRV_PCM_HW_PARAM_RATE, mProxyPcmHandle->rate);

    param_set_hw_refine(mProxyPcmHandle, params);

    if (param_set_hw_params(mProxyPcmHandle, params)) {
        LOGE("Failed to set hardware params on Proxy device");
        param_dump(params);
        goto bail;
    }

    param_dump(params);
    mProxyPcmHandle->buffer_size = pcm_buffer_size(params);
    mProxyPcmHandle->period_size = pcm_period_size(params);
    mProxyPcmHandle->period_cnt  = mProxyPcmHandle->buffer_size/mProxyPcmHandle->period_size;
    sparams = (struct snd_pcm_sw_params*) malloc(sizeof(struct snd_pcm_sw_params));
    if (!sparams) {
        LOGE("Failed to allocated software params for Proxy device");
        goto bail;
    }
   sparams->tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
   sparams->period_step = 1;
   sparams->avail_min = (mProxyPcmHandle->flags & PCM_MONO) ?
       mProxyPcmHandle->period_size/2 : mProxyPcmHandle->period_size/4;
   sparams->start_threshold = 1;
   sparams->stop_threshold = (mProxyPcmHandle->flags & PCM_MONO) ?
       mProxyPcmHandle->buffer_size/2 : mProxyPcmHandle->buffer_size/4;
   sparams->xfer_align = (mProxyPcmHandle->flags & PCM_MONO) ?
       mProxyPcmHandle->period_size/2 : mProxyPcmHandle->period_size/4; /* needed for old kernels */
   sparams->silence_size = 0;
   sparams->silence_threshold = 0;

   if (param_set_sw_params(mProxyPcmHandle, sparams)) {
        LOGE("Failed to set software params on Proxy device");
        goto bail;
   }
   if (pcm_prepare(mProxyPcmHandle)) {
       LOGE("Failed to pcm_prepare on Proxy device");
       goto bail;
   }
   if(params) delete params;
   if(sparams) delete sparams;
   return NO_ERROR;

bail:
   if(mProxyPcmHandle) pcm_close(mProxyPcmHandle);
   if(params) delete params;
   if(sparams) delete sparams;
   return NO_INIT;
}

status_t AudioSessionOutALSA::openDevice(char *useCase, bool bIsUseCase, int devices)
{
    alsa_handle_t alsa_handle;
    status_t status = NO_ERROR;
    LOGD("openDevice: E");
    alsa_handle.module      = mALSADevice;
    alsa_handle.bufferSize  = DEFAULT_BUFFER_SIZE;
    alsa_handle.devices     = devices;
    alsa_handle.handle      = 0;
    alsa_handle.format      = SNDRV_PCM_FORMAT_S16_LE;
    alsa_handle.channels    = mChannels;
    alsa_handle.sampleRate  = mSampleRate;
    alsa_handle.mode        = mParent->mode();
    alsa_handle.latency     = PLAYBACK_LATENCY;
    alsa_handle.rxHandle    = 0;
    alsa_handle.ucMgr       = mUcMgr;

    strlcpy(alsa_handle.useCase, useCase, sizeof(alsa_handle.useCase));
    char *ucmDevice = mALSADevice->getUCMDevice(devices & AudioSystem::DEVICE_OUT_ALL, 0);
    if(bIsUseCase) {
        snd_use_case_set_case(mUcMgr, "_verb", alsa_handle.useCase, ucmDevice);
    } else {
        snd_use_case_set_case(mUcMgr, "_enamod", alsa_handle.useCase, ucmDevice);
    }

    status = mALSADevice->open(&alsa_handle);
    if(status != NO_ERROR) {
        LOGE("Could not open the ALSA device for use case %s", alsa_handle.useCase);
        mALSADevice->close(&alsa_handle);
    } else{
        mParent->mDeviceList.push_back(alsa_handle);
    }
    LOGD("openDevice: X");
    return status;
}

status_t AudioSessionOutALSA::closeDevice(alsa_handle_t *pHandle)
{
    status_t status = NO_ERROR;
    LOGV("closeDevice: useCase %s", pHandle->useCase);
    if(pHandle) {
        status = mALSADevice->close(pHandle);
    }
    return status;
}

status_t AudioSessionOutALSA::doRouting(int devices)
{
    status_t status = NO_ERROR;
    char *use_case;
    char *ucmDevice = mALSADevice->getUCMDevice(devices & AudioSystem::DEVICE_OUT_ALL, 0);

    LOGV("doRouting: devices 0x%x", devices);
    mDevices = devices;
    if(mDevices & AudioSystem::DEVICE_OUT_ALL_A2DP) {
        mCaptureFromProxy = true;
        mRouteAudioToA2dp = true;
        mDevices &= ~AudioSystem::DEVICE_OUT_ALL_A2DP;
        //ToDo: Handle A2dp+Speaker
        //mDevices |= AudioSystem::DEVICE_OUT_PROXY;
        mDevices = AudioSystem::DEVICE_OUT_PROXY;
    } else {
        mRouteAudioToA2dp = false;
        mCaptureFromProxy = false;
    }
    if(mPcmRxHandle && devices != mPcmRxHandle->devices) {
        mALSADevice->route(mPcmRxHandle, devices, mParent->mode());
        //snd_use_case_set_case(mUcMgr, "_swdev", mPcmRxHandle->useCase, ucmDevice);
        mPcmRxHandle->devices = devices;
    }
    if(mUseTunnelDecode) {
        mALSADevice->route(mCompreRxHandle, devices, mParent->mode());
        mCompreRxHandle->devices = devices;
    } else if(mRouteCompreToSpdif && !(devices & AudioSystem::DEVICE_OUT_SPDIF)) {
        mRouteCompreToSpdif = false;
        status = closeDevice(mCompreRxHandle);
    } else if(!mRouteCompreToSpdif && (devices & AudioSystem::DEVICE_OUT_SPDIF)) {
        snd_use_case_get(mUcMgr, "_verb", (const char **)&use_case);
#if 0
        if ((use_case == NULL) || (!strcmp(use_case, SND_USE_CASE_VERB_INACTIVE))) {
            status = openDevice(SND_USE_CASE_VERB_HIFI_COMPRESSED, true, AudioSystem::DEVICE_OUT_SPDIF);
        } else {
            status = openDevice(SND_USE_CASE_MOD_PLAY_MUSIC_COMPRESSED, false, AudioSystem::DEVICE_OUT_SPDIF);
        }
#endif
        free(use_case);
        if(status == NO_ERROR) {
            mRouteCompreToSpdif = true;
        }
    }
    if(mCaptureFromProxy) {
        if(!mProxyPcmHandle) {
            status = openProxyDevice();
        }
        if(status == NO_ERROR && mRouteAudioToA2dp && !mA2dpDevice) {
            status = openA2dpOutput();
            if(status == NO_ERROR) {
                status = startA2dpOutput();
            }
        }
    } else {
        if(mProxyPcmHandle) {
            LOGV("Closing the Proxy device");
            pcm_close(mProxyPcmHandle);
            mProxyPcmHandle = NULL;
        }
        if(mA2dpOutputStarted) {
            status = stopA2dpOutput();
            mA2dpOutputStarted = false;
            closeA2dpOutput();
        }
    }
    return status;
}

}       // namespace android_audio_legacy
