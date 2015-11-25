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

#ifndef __PID_LOCK_H__
#define __PID_LOCK_H__

#include <sys/types.h>

/*******************************************************************************
 * A cooperative lock for a singleton instance of process by name. Provides bsd
 * like pidfile operations to ensure there is atmost one instance of the daemon
 * process on this host.
 ******************************************************************************/

/*******************************************************************************
 * release the pidfile lock
 * @param path
 * @param pidfd
 ******************************************************************************/
void pidfile_release(const char* path, int* pidfd);

/*******************************************************************************
 * acquire the pidfile. on success the process id of current will be written
 * in to the file. On a return value EEXIST, it reports the process id of the
 * file owning the pidfile in to *pidptr.
 *
 * @param path
 * @param pidptr : return the process id of the owner.
 * @param pidfd : on success will have a valid fd. on failure will be -1.
 *
 * @return int : 0 or the errno. EEXIST if another process owned the lock.
 ******************************************************************************/
int pidfile_acquire(const char* path, pid_t* pidptr, int* pidfd);

#endif /* !__PID_LOCK_H__ */

