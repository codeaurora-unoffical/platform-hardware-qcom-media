/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * this software is provided "as is" and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement
 * are disclaimed.  in no event shall the copyright owner or contributors
 * be liable for any direct, indirect, incidental, special, exemplary, or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or
 * business interruption) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence
 * or otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <linux/msm_ion.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "C2DColorConverter.h"

#define SZ_4K 0x1000

using namespace android;

struct ion_buf {
    int ion_device_fd;
    struct ion_fd_data fd_data;
    struct ion_allocation_data alloc_data;
    void *buf;
};

void *ion_mmap(int size, struct ion_allocation_data *alloc_data,
        struct ion_fd_data *fd_data)
{
    int ion_device_fd = -1 , rc = 0, ion_dev_flags = 0;
    void *ret;

    if (size <= 0 || !alloc_data || !fd_data) {
        printf("Invalid input to alloc_map_ion_memory\n");
        return NULL;
    }

    ion_dev_flags = O_RDONLY;
    ion_device_fd = open("/dev/ion", ion_dev_flags);
    if (ion_device_fd < 0) {
        printf("ERROR: ION Device open() Failed\n");
        return NULL;
    }

    alloc_data->len = (size + (SZ_4K - 1)) & ~(SZ_4K - 1);
    alloc_data->align = SZ_4K;
    alloc_data->flags = ION_FLAG_CACHED;
    alloc_data->heap_mask = (ION_HEAP(ION_CP_MM_HEAP_ID) |
                             ION_HEAP(ION_IOMMU_HEAP_ID));
    printf("ION ALLOC unsec buf: size %d align %d flags %x\n",
           (int)alloc_data->len, (int)alloc_data->align, alloc_data->flags);

    rc = ioctl(ion_device_fd, ION_IOC_ALLOC, alloc_data);
    if (rc || !alloc_data->handle) {
        printf("ION ALLOC memory failed 0x%x\n", rc);
        alloc_data->handle =NULL;
        close(ion_device_fd);
        ion_device_fd = -1;
        return NULL;
    }
    fd_data->handle = alloc_data->handle;
    rc = ioctl(ion_device_fd, ION_IOC_SHARE, fd_data);
    if (rc) {
        printf("ION SHARE failed\n");
    }

    ret = mmap(NULL, alloc_data->len, PROT_READ|PROT_WRITE,
               MAP_SHARED, fd_data->fd, 0);
    if (ret == MAP_FAILED) {
        printf("mmap failed for ion!\n");
        return NULL;
    }

    return ret;
}

int main(int argc, char *argv[]) {
    int fd_src, fd_dst, len;
    void *mLibHandle;
    createC2DColorConverter_t *mConvertOpen;
    destroyC2DColorConverter_t *mConvertClose;
    C2DColorConverterBase* c2dcc;
    struct ion_buf input_buf, output_buf;
    C2DBuffReq req_in, req_out;
    int ret = -1;
    struct timespec tstart={0,0}, tend={0,0};

    // Load C2D library and symbols
    dlopen("libm.so", RTLD_NOW|RTLD_GLOBAL);
    mLibHandle = dlopen("libc2dcolorconvert.so", RTLD_NOW);
    if (!mLibHandle) {
        printf("Library loading failed!\n");
        return -1;
    }
    printf("Loaded libc2dcolorconvert.so\n");
    mConvertOpen = (createC2DColorConverter_t *)
        dlsym(mLibHandle, "createC2DColorConverter");
    mConvertClose = (destroyC2DColorConverter_t *)
        dlsym(mLibHandle,"destroyC2DColorConverter");
    c2dcc = mConvertOpen(3840, 2160, 1280, 720, NV12_128m, NV12_128m, 0, 0);
    if (!c2dcc) {
        printf("Failed getting C2D object!\n");
        return -1;
    }

    // Get the buffer requirements
    ret = c2dcc->getBuffReq(C2D_INPUT, &req_in);
    if (ret) {
        printf("Failed to get C2D input buffer requirements!\n");
        return -1;
    }
    printf("C2D input buffer requirements: height %d width %d size %d\n",
           req_in.height, req_in.width, req_in.size);

    ret = c2dcc->getBuffReq(C2D_OUTPUT, &req_out);
    if (ret) {
        printf("Failed to get C2D output buffer requirements!\n");
        return -1;
    }
    printf("C2D output buffer requirements: height %d width %d size %d\n",
           req_out.height, req_out.width, req_out.size);

    // Allocate ION buffers
    input_buf.buf = ion_mmap(req_in.size, &input_buf.alloc_data,
                             &input_buf.fd_data);
    output_buf.buf = ion_mmap(req_out.size, &output_buf.alloc_data,
                              &output_buf.fd_data);

    // Read input file into ION buffer
    fd_src = open(argv[1], O_RDWR);
    if (!fd_src) {
        printf("Failed opening input file!\n");
        return -1;
    }
    len = read(fd_src, input_buf.buf, req_in.size);
    printf("Read %d bytes from the file %s\n", len, argv[1]);
    close(fd_src);

    // Downscale input in C2D
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    ret = c2dcc->convertC2D(input_buf.fd_data.fd, input_buf.buf, input_buf.buf,
                            output_buf.fd_data.fd, output_buf.buf, output_buf.buf);
    if (ret) {
        printf("C2D conversion failed!\n");
        return -1;
    }
    clock_gettime(CLOCK_MONOTONIC, &tend);

    printf("Downscaling took %.6f seconds\n",
           ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) -
           ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec));

    // Dump C2D output to file
    fd_dst = open(argv[2], O_RDWR);
    if (!fd_dst) {
        printf("Failed opening output file!\n");
        return -1;
    }
    len = write(fd_dst, output_buf.buf, req_out.size);
    if (len < req_out.size) {
        printf("Failed writing the output file!\n");
        return -1;
    }
    close(fd_dst);
    mConvertClose(c2dcc);

    // TODO: Free the ION buffers and close the libraries
}
