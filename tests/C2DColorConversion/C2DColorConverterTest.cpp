/*
 * Copyright (C) 2010 The Android Open Source Project
 *
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
/*--------------------------------------------------------------------------
Copyright (c) 2012, The Linux Foundation. All rights reserved.
--------------------------------------------------------------------------*/

#include <cutils/memory.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <surfaceflinger/Surface.h>
#include <surfaceflinger/ISurface.h>
#include <surfaceflinger/SurfaceComposerClient.h>

#include <gralloc_priv.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <qcom_ui.h>
#include <dlfcn.h>
#include <MediaDebug.h>

#include <C2DColorConverter.h>

#define ALIGN( num, to ) (((num) + (to-1)) & (~(to-1)))
#define ALIGN8K 8192
#define ALIGN4K 4096
#define ALIGN2K 2048
#define ALIGN128 128
#define ALIGN32 32
#define ALIGN16 16

using namespace android;

int64_t getTimeOfDayUs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

int32_t getFormatSize(int32_t format, int32_t width, int32_t height)
{
    switch (format) {
        case 1: // RGB565
            return (ALIGN (width, ALIGN32) * ALIGN (height , ALIGN32) * 2);
        case 2: // TILE 4x2
            return (ALIGN(ALIGN(width, ALIGN128) * ALIGN(height, ALIGN32), ALIGN8K) + ALIGN(ALIGN(width, ALIGN128)* ALIGN(height/2, ALIGN32), ALIGN8K));
        case 3: // sp
            return ALIGN((ALIGN(width, ALIGN32) * height) + (ALIGN(width/2, ALIGN32) * (height/2) * 2), ALIGN4K);
        default:
            return 0;
    }
}

int32_t getHalFormat(int32_t format)
{
    switch (format) {
        case 1: // RGB565
            return HAL_PIXEL_FORMAT_RGB_565;
        case 2: // TILE 4x2
            return HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED;
        case 3: // sp
            return HAL_PIXEL_FORMAT_YCbCr_420_SP;
        default:
            return 0;
    }
}

int32_t getStride(int32_t format, int32_t width)
{
    switch (format) {
        case 1: // RGB565
            return ALIGN(width, ALIGN32) * 2;
        case 2: // TILE 4x2
            return ALIGN(width, ALIGN128);
        case 3: // sp
            return ALIGN(width, ALIGN32);
        default:
            return 0;
    }
}

int32_t getSlice(int32_t format, int32_t height)
{
    switch (format) {
        case 1: // RGB565
            return ALIGN(height, ALIGN32);
        case 2: // TILE 4x2
            return ALIGN(height, ALIGN32);
        case 3: // sp
            return height;
        default:
            return 0;
    }
}

int main(int argc, char** argv)
{

    if (argc < 6) {
        printf("usage: ./test-C2D filename w h formatIn formatOut"
               "(1 - RGB565, 2 - TILE4x2, 3 - sp) delay in ms (default is 33)\n");
        return -1;
    }

    if (atoi(argv[2]) < 0 || atoi(argv[3]) < 0) {
        printf("invalid w or h\n");
        return -1;
    }

    char * filename = argv[1];
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    int delay = 33;
    int inFormat = atoi(argv[4]);
    int outFormat = atoi(argv[5]);
    int inSize = getFormatSize(inFormat, width, height);
    int outSize = getFormatSize(outFormat, width, height);
    int outStride = getStride(outFormat, width);
    int outSlice = getSlice(outFormat, height);
    CHECK(inSize);
    CHECK(outSize);

    if (argc > 6) {
        delay = atoi(argv[6]);
    }

    int fd = -1;
    if ((fd = open(filename, O_RDONLY)) == -1) {
        printf("file open failed with errno %d\n", errno);
        return -1;
    }

    // create a client to surfaceflinger
    sp<SurfaceComposerClient> client = new SurfaceComposerClient();

    sp<SurfaceControl> surfaceControl = client->createSurface(
                                             String8("test-C2D"),
                                             0,
                                             600, 1024,
                                             PIXEL_FORMAT_RGB_565);

    SurfaceComposerClient::openGlobalTransaction();
    CHECK(!surfaceControl->setLayer(100000));
    CHECK(!surfaceControl->show(100000));
    SurfaceComposerClient::closeGlobalTransaction();

    // pretend it went cross-process
    Parcel parcel;
    SurfaceControl::writeSurfaceToParcel(surfaceControl, &parcel);
    parcel.setDataPosition(0);
    sp<Surface> surface = Surface::readFromParcel(parcel);
    ANativeWindow* window = surface.get();
    window->setSwapInterval(window, 1);
    printf("window=%p\n", window);

    ANativeWindowBuffer* buffer;
    ANativeWindowBuffer* buffer_out;

    int err = native_window_set_buffers_geometry(
                  window,
                  outStride,
                  outSlice,
                 getHalFormat(outFormat));

    android_native_rect_t crop;
    crop.left = 0;
    crop.top = 0;
    crop.right = width;
    crop.bottom = height;

    err = native_window_set_crop(window, &crop);

    err = native_window_set_buffer_count(window, 4);

    err = native_window_set_usage(
                          window,
                          GRALLOC_USAGE_PRIVATE_MM_HEAP |
                          GRALLOC_USAGE_PRIVATE_UNCACHED |
                          GRALLOC_USAGE_HW_TEXTURE |
                          GRALLOC_USAGE_EXTERNAL_DISP);

    if (inSize > outSize) {
        outSize = inSize;
    }
    err = window->perform(window, NATIVE_WINDOW_SET_BUFFERS_SIZE, outSize);

    int64_t start_time, end_time;

    int count = -1;
    int64_t frame_count = 0;
    int64_t read_time = 0;
    int64_t lock_time = 0;
    int64_t queue_time = 0;
    int64_t dequeue_time = 0;
    int64_t conversion_time = 0;
    count = 30;

    void *mLibHandle = dlopen("libc2dcolorconvert.so", RTLD_LAZY);
    createC2DColorConverter_t *mConvertOpen = NULL;
    destroyC2DColorConverter_t *mConvertClose = NULL;
    C2DColorConverterBase * C2DCC = NULL;

    if (mLibHandle) {
        mConvertOpen = (createC2DColorConverter_t *)dlsym(mLibHandle,"createC2DColorConverter");
        mConvertClose = (destroyC2DColorConverter_t *)dlsym(mLibHandle,"destroyC2DColorConverter");
        if(mConvertOpen != NULL && mConvertClose != NULL) {
            printf("Successfully acquired mConvert symbol");
            C2DCC = mConvertOpen(width, height, width, height, (ColorConvertFormat)inFormat, (ColorConvertFormat)outFormat, 0);
        } else {
            printf("Could not get the mConverts...\n");
            CHECK(0);
        }
    }
    else {
        printf("Could not get yuvconversion lib handle");
        CHECK(0);
    }

    for (int i=0 ; i<count ; i++) {
        int64_t dq, lk, rd, q, cn;

        start_time = getTimeOfDayUs();
        err = window->dequeueBuffer(window, &buffer);
        dq = getTimeOfDayUs() - start_time;
        dequeue_time += dq;

        if (err != 0) {
            printf("dequeueBuffer failed with err 0x%08x\n", err);
            continue;
        }

        private_handle_t const* hnd =
            reinterpret_cast<private_handle_t const*>(buffer->handle);

        int size = hnd->size;
        sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(buffer,
                                                      false));
        unsigned char * addr = NULL;
        Region region(Rect(0, 0,
                           graphicBuffer->getWidth(),
                           graphicBuffer->getHeight()));
        int64_t lock_start = getTimeOfDayUs();
        graphicBuffer->lock(GRALLOC_USAGE_SW_READ_OFTEN |
                            GRALLOC_USAGE_SW_WRITE_OFTEN,
                            region.bounds(),
                            (void **)&addr);
        lk = getTimeOfDayUs() - lock_start;
        lock_time += lk;
        int copySize = inSize;

        if (copySize > size) {
            printf("insufficient buffer size, copySize = %d, size = %d\n",
                   copySize, size);
            graphicBuffer->unlock();
            graphicBuffer.clear();
            goto cleanup;
        }

        int64_t read_start, read_stop;
        read_start = getTimeOfDayUs();
        if (read(fd, addr, copySize) < 0) {
            printf("read from file failed with errno %d\n", errno);
            graphicBuffer->unlock();
            graphicBuffer.clear();
            goto cleanup;
        }
        read_stop = getTimeOfDayUs();
        rd = read_stop - read_start;
        read_time += rd;

        err = window->dequeueBuffer(window, &buffer_out);
        if (err != 0) {
            printf("dequeueBuffer failed with err 0x%08x\n", err);
            window->cancelBuffer(window, buffer);
            graphicBuffer->unlock();
            graphicBuffer.clear();
            continue;
        }

        private_handle_t const* hnd_out =
            reinterpret_cast<private_handle_t const*>(buffer_out->handle);

        sp<GraphicBuffer> graphicBuffer_out(new GraphicBuffer(buffer_out,
                                                      false));
        unsigned char * addr_out = NULL;
        Region region_out(Rect(0, 0,
                           graphicBuffer_out->getWidth(),
                           graphicBuffer_out->getHeight()));

        graphicBuffer_out->lock(GRALLOC_USAGE_SW_READ_OFTEN |
                            GRALLOC_USAGE_SW_WRITE_OFTEN,
                            region_out.bounds(),
                            (void **)&addr_out);

        int conversionTime_start = getTimeOfDayUs();
        int result =  C2DCC->convertC2D(hnd->fd, (void *)addr, hnd_out->fd, (void *)addr_out);
        int conversionTime_end = getTimeOfDayUs();
        cn = conversionTime_end - conversionTime_start;
        conversion_time += cn;

        graphicBuffer->unlock();
        graphicBuffer.clear();
        graphicBuffer_out->unlock();
        graphicBuffer_out.clear();
        window->cancelBuffer(window, buffer);

        if (result < 0) {
            printf("Conversion failed\n");
            window->cancelBuffer(window, buffer_out);
            goto cleanup;
        }

        int64_t queue_start = getTimeOfDayUs();
        window->queueBuffer(window, buffer_out);
        end_time = getTimeOfDayUs();
        q = end_time - queue_start;
        queue_time += q;
        ++frame_count;

        if (delay <  0) continue;
        else {
            int64_t timeTaken = end_time - start_time;
            int64_t timePending = delay*1000 - timeTaken;
            if (timePending <= 0) {
                printf("loop < 0 culprits dq %lld, lock %lld, "
                        "read %lld, q %lld, cn %lld\n",
                        dq, lk, rd, q, cn);
                continue;
            } else {
                usleep(timePending);
            }
        }
    }

 cleanup:
    if (frame_count) {
        printf("Number of frames queued %lld\n",frame_count);
        printf("avg dequeue time %lld\n", dequeue_time/frame_count);
        printf("avg read time %lld\n", read_time/frame_count);
        printf("avg queue time %lld\n", queue_time/frame_count);
        printf("avg lock time %lld\n", lock_time/frame_count);
        printf("avg conversion time %lld\n",conversion_time/frame_count);
    }

    mConvertClose(C2DCC);
    dlclose(mLibHandle);

    close(fd);

    printf("test complete\n");
    return 0;
}
