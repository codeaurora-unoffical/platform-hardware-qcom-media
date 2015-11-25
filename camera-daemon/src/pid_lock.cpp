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
#include "pid_lock.h"
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

/*******************************************************************************
 * release the pidfile lock
 * @param path
 * @param pidfd
 ******************************************************************************/
void pidfile_release(const char* path, int* pidfd)
{
    if (-1 != *pidfd) {
        (void) unlink(path);
        (void) lockf(*pidfd, F_ULOCK, 0);
        (void) close(*pidfd);
        *pidfd = -1;
    }
}

/*******************************************************************************
 * read the process id in the pidfile
 *
 * @param path
 * @param pidptr
 *
 * @return int
 ******************************************************************************/
static int pidfile_read(const char* path, pid_t* pidptr)
{
    int fd = -1;
    char buf[16];
    int nread;
    char* endptr;
    int nret = 0;

    fd = open(path, O_RDONLY);
    if (-1 == fd) {
        nret = errno;
        goto bail;
    }

    nread = read(fd, buf, sizeof(buf)-1);
    if (-1 == nread) {
        nret = errno;
        goto bail;
    }

    *pidptr = strtol(buf, &endptr, 10);   /* extract the pid */

bail:
    if (-1 != fd) {
        close(fd);
    }
    return nret;
}

/*******************************************************************************
 * acquire the pidfile. on success the process id of current will be written
 * in to the file. On a return value EEXIST, it reports the process id of the
 * file owning the pidfile in to *pidptr.
 *
 * @param path
 * @param pidptr
 * @param pidfd : on success will have a valid fd. on failure will be -1.
 *
 * @return int
 ******************************************************************************/
int pidfile_acquire(const char* path, pid_t* pidptr, int* pidfd)
{
    int nret = 0;
    int fd;
    char buf[16];
    int nwritten;

    *pidfd = -1;   /* invalid fd */

    /* exclusive file open, @note this is inheritable lock */
    fd = open(path, O_WRONLY | O_CREAT, 0600);
    if (-1 == fd) {
        nret = errno;
        goto bail;
    }

    nret = lockf(fd, F_TLOCK, 0);
    if (-1 == nret) {

        nret = errno;
        close(fd); fd = -1;

        if (EAGAIN == nret || EACCES == nret) {   /* read the pid from file. */

            nret = pidfile_read(path, pidptr);
            if (0 == nret) {
                nret = EEXIST;   /* report the pidfile exists */
            }
        }
        goto bail;
    }

    nret = ftruncate(fd, 0);
    if (-1 == nret) {
        nret = errno;
        goto bail_2;
    }

    *pidptr = getpid();

    nwritten = snprintf(buf, sizeof(buf), "%u\n", *pidptr);
    if (nwritten < 0 || sizeof(buf) < (size_t)nwritten) {
        nret = EINVAL;
        goto bail_2;
    }

    if (nwritten != write(fd, buf, nwritten)) {
        nret = errno;
        goto bail_2;
    }

    *pidfd = fd;
    nret = 0;

bail_2:

    if (0 != nret) {
        pidfile_release(path, &fd);
    }

bail:
    return nret;
}

