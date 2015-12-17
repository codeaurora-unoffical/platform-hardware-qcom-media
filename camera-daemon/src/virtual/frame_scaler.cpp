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
#include "frame_scaler.h"
#include <string.h>

/** TODO: fix this convoluted path */
#include "../../libc2dcolorconvert/C2DColorConverter.h"
#include <dlfcn.h>

namespace camerad
{

/**
 Performs frame duplication.
 **/
class FrameReplicate : public ICameraFrameScaler {
protected:
    FrameReplicate() {}
    FrameReplicate(const FrameReplicate&) = delete;
    const FrameReplicate& operator =(const FrameReplicate&) = delete;

    static std::shared_ptr<FrameReplicate> make_shared() {
        struct make_shared_enabler : public FrameReplicate {};
        return std::make_shared<make_shared_enabler>();
    }
public:
    /* duplicate */
    virtual int convert(camera::ICameraFrame* srcFrame,
                        camera::ICameraFrame* outFrame);
    static int create(ICameraFrameScalerPtr* pa);
};

/**
 Performs down scaling of a given camera frame.
 **/
class C2DScaler : public ICameraFrameScaler {
    uint32_t inWidth_;
    uint32_t inHeight_;
    uint32_t outWidth_;
    uint32_t outHeight_;

    /**
     Internal class to interface with the shared library "libc2dcolorconvert.so".
     **/
    class c2dlib {
        android::C2DColorConverterBase *c2dcc_;
        void* handle_;

    public:
        int init(uint32_t inWidth, uint32_t inHeight,
                 uint32_t outWidth, uint32_t outHeight) {
            int rc = 0;

            handle_ = dlopen("libc2dcolorconvert.so", RTLD_NOW);

            if (0 != handle_) {
                android::createC2DColorConverter_t *createOp =
                    (android::createC2DColorConverter_t*) dlsym(
                        handle_, "createC2DColorConverter");
                c2dcc_ = createOp(inWidth, inHeight, outWidth, outHeight,
                                  android::NV12_128m, android::NV12_128m, 0, 0);
            }
            else {
                rc = ELIBEXEC;
            }

            return rc;
        }

        void final(void) {

            if (0 != handle_) {

                if (0 != c2dcc_) {
                    android::destroyC2DColorConverter_t *destroyOp =
                        (android::destroyC2DColorConverter_t*) dlsym(
                            handle_, "destroyC2DColorConverter");
                    destroyOp(c2dcc_); c2dcc_ = 0;
                }
                dlclose(handle_); handle_ = 0;
            }
        }

        inline int scale(int srcFd, void* srcBase, void* srcData, int dstFd,
                       void* dstBase, void* dstData) {
           return c2dcc_->convertC2D(srcFd, srcBase, srcData,
                                     dstFd, dstBase, dstData);
        }
    } c2dlib_;

protected:
    C2DScaler() {}
    int init(uint32_t inWidth, uint32_t inHeight,
             uint32_t outWidth, uint32_t outHeight);

    C2DScaler(const C2DScaler&) = delete;
    const C2DScaler& operator =(const C2DScaler&) = delete;

    static std::shared_ptr<C2DScaler> make_shared() {
        struct make_shared_enabler : public C2DScaler {};
        return std::make_shared<make_shared_enabler>();
    }
public:
    virtual ~C2DScaler();
    virtual int convert(camera::ICameraFrame* srcFrame,
                        camera::ICameraFrame* outFrame);
    static int create(uint32_t inWidth, uint32_t inHeight,
                      uint32_t outWidth, uint32_t outHeight,
                      ICameraFrameScalerPtr* pa);
};

/* duplicate */
int FrameReplicate::convert(camera::ICameraFrame* srcFrame,
                            camera::ICameraFrame* outFrame)
{
    memmove(outFrame->data, srcFrame->data,
            std::min(outFrame->size, srcFrame->size));
    return 0;
}

int FrameReplicate::create(ICameraFrameScalerPtr* pa)
{
    return (nullptr == (*pa = make_shared())) ? ENOMEM : 0;
}

int C2DScaler::convert(camera::ICameraFrame* srcFrame,
                       camera::ICameraFrame* outFrame)
{
    return c2dlib_.scale(srcFrame->fd, srcFrame->data, srcFrame->data,
                         outFrame->fd, outFrame->data, outFrame->data);
}

C2DScaler::~C2DScaler()
{
    c2dlib_.final();
}

int C2DScaler::init(uint32_t inWidth, uint32_t inHeight,
                    uint32_t outWidth, uint32_t outHeight)
{

    inWidth_ = inWidth;
    inHeight_ = inHeight;
    outWidth_ = outWidth;
    outHeight_ = outHeight;

    return c2dlib_.init(inWidth, inHeight, outWidth, outHeight);
}

int C2DScaler::create(uint32_t inWidth, uint32_t inHeight,
                      uint32_t outWidth, uint32_t outHeight,
                      ICameraFrameScalerPtr* pa)
{
    std::shared_ptr<C2DScaler> a = make_shared();
    *pa = a;
    return (nullptr != a) ? a->init(inWidth, inHeight, outWidth, outHeight)
                : ENOMEM;
}

/**
 Create an object supporting suitable frame scaling operations.

 @param inWidth
 @param inHeight
 @param outWidth
 @param outHeight
 @param pa

 @return int
 **/
int ICameraFrameScaler::create(uint32_t inWidth, uint32_t inHeight,
                               uint32_t outWidth, uint32_t outHeight,
                               ICameraFrameScalerPtr* pa)
{
    return (inWidth == outWidth && inHeight == outHeight)
    ? FrameReplicate::create(pa) : C2DScaler::create(inWidth, inHeight,
                                                     outWidth, outHeight,
                                                     pa);
}

}

