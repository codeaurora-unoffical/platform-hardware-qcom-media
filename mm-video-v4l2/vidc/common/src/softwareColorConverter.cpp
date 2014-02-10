/*--------------------------------------------------------------------------
Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------------------------------------*/

#include <softwareColorConverter.h>
#include <cutils/log.h>
#include <dlfcn.h>

softwareColorConverter::softwareColorConverter() {
    // Open the shared library
    mLibraryHandle = dlopen("libI420colorconvert.so", RTLD_NOW);

    if (mLibraryHandle == NULL) {
        ALOGW("I420ColorConverter: cannot load libI420colorconvert.so");
        return;
    }

    // Find the entry point
    void (*getI420ColorConverter)(softwareColorConverter *converter) =
        (void (*)(softwareColorConverter*)) dlsym(mLibraryHandle, "getI420ColorConverter");

    if (getI420ColorConverter == NULL) {
        ALOGW("I420ColorConverter: cannot load getI420ColorConverter");
        dlclose(mLibraryHandle);
        mLibraryHandle = NULL;
        return;
    }

    // Fill the function pointers.
    getI420ColorConverter(this);

    ALOGI("I420ColorConverter: libI420colorconvert.so loaded");
}

bool softwareColorConverter::isInitialized() {
    if (mLibraryHandle) {
        ALOGI("softwareColorConverter: Sucessfully initialized");
        return true;
    } else {
        ALOGW("softwareColorConverter:: Initialization failed");
        return false;
    }
}

softwareColorConverter::~softwareColorConverter() {
    if (mLibraryHandle) {
        dlclose(mLibraryHandle);
    }
}
