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
#include "file_component.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <queue>
#include <condition_variable>

using namespace omxa;

class omxa::OmxFileSink : public OmxSink {
    friend class FileComponent;
    std::ofstream fs_;
    std::thread writer_;  /**< writer thread to drain the buffer contents to ostream_ */
    std::condition_variable cv_;
    std::queue<OMX_BUFFERHEADERTYPE*>  pending_;   /**< queue of buffers pending to be written */
    bool opened_ = false;
    uint32_t pending_count_ = 0; /* debug purposes */
    uint32_t pending_max_count_ = 0; /* debug purposes */

    /**
     writer thread lingers in this subroutine and drains buffers in to file.
     thread shuts down when opened_ flag is cleared.
     **/
    void doWrite() {
        std::unique_lock<std::mutex> lk(lock_);
        OMX_BUFFERHEADERTYPE* buffer = NULL;

        while (opened_) {
            /** wait until the pending_ list is non-empty; interrupt if no
             *  longer opened */
            cv_.wait(lk, [this](){return !pending_.empty() || !opened_;});

            if (!opened_) {
                break;
            }

            /* drain this buffer */
            buffer = pending_.front();
            pending_.pop();
            pending_count_--;

            lk.unlock();  /* unlock for the write and release operations */

            fs_.write((const char *)buffer->pBuffer, buffer->nFilledLen);

            /* release it to encoder */
            (void)OMX_FillThisBuffer(source_, buffer);

            lk.lock();
        }
    }

    /**
     Open an output stream to the given path. Associate the stream with the
     source component. close and re-open if previously opened to a different
     path.
     @param [in] hComponent : source component. buffers when drained will be
                        released back to this omx component
     @param [in] path : path to a file, must be a writeable.
     @return int : 0 when successfuly opened, or EIO.
     **/
    int open(OMX_HANDLETYPE hComponent, const char* path) {
        std::unique_lock<std::mutex> lk(lock_);

        close_locked();
        if (writer_.joinable()) {
            /* ensure the writer thread can wake up and return */
            lk.unlock();
            cv_.notify_all();
            writer_.join();
            lk.lock();
        }

        source_ = hComponent;
        if (!opened_) {
            fs_.open(path, std::ios::out | std::ios::trunc | std::ios::binary);
            opened_ = fs_.is_open();

            if (opened_) {
                writer_ = std::thread(&omxa::OmxFileSink::doWrite, this);
            }
            else {
                /* failed to open */
                close_locked();
                return EIO;
            }
        }
        return 0;
    }

    void close_locked(void) {

        source_ = NULL;
        fs_.close();
        opened_ = false;
    }

public:
    virtual void close() {
        std::unique_lock<std::mutex> lk(lock_);
        close_locked();
        if (writer_.joinable()) {
            /* ensure the writer thread can wake up and return */
            lk.unlock();
            cv_.notify_all();
            writer_.join();
        }
    }

    virtual ~OmxFileSink() { close(); }

    virtual bool isOpen() {
        return opened_;
    }

    /**
     Hold on to the OMXBUFFERHEADER and asynchronously write in a separate
     thread
     @param buf
     **/
    virtual OMX_ERRORTYPE emptyBuffer(OMX_BUFFERHEADERTYPE* pBuffer) {

        if (0 == pBuffer->nFilledLen) {   /* no data */
            return OMX_FillThisBuffer(source_, pBuffer);
        }

        std::unique_lock<std::mutex> lk(lock_);
        pending_.push(pBuffer);

        pending_count_++;
        if (pending_max_count_ < pending_count_) { /* capture buffer demand */
            pending_max_count_ = pending_count_;
        }

        lk.unlock();
        cv_.notify_one();

        return OMX_ErrorNone;
    }
};

FileComponent::~FileComponent(){}

static int create_h264_file(std::string& fpath)
{
    int fd;
    int rc = 0;

    fpath.append(".h264");
    fd = open(fpath.c_str(), O_RDWR | O_CREAT | O_EXCL,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (-1 == fd) {
        rc = errno;
    }
    else {
        close(fd);
    }
    return rc;
}

int FileComponent::openOMXSink(OMX_HANDLETYPE hComponent, OmxSinkPtr* ppout)
{
    int nret = 0;
    if (sink_->isOpen()) {
        return EALREADY;
    }

    nret = sink_->open(hComponent, path_.c_str());
    if (0 == nret) {
        *ppout = sink_;
    }

    return nret;
}

/*******************************************************************************
 * concatenate the file to the directory, make sure there is just one directory
 * separator.
 *
 * @param dir
 * @param file
 ******************************************************************************/
static void append_path(std::string& dir, const char* file)
{
    int len = dir.length();

    if (0 != len && '/' != dir[len-1]) {
        dir.append("/");
    }
    if ('/' == *file) {
        file++;
    }
    dir.append(file);
}

int FileComponent::init(const char* path)
{
    std::time_t t = std::time(nullptr);
    int pos;
    int rc = 0;
    char fmt_time_str[64];    /* todo: review the size when locale is changed,
                               ** especially the multibyte lang. */

    path_ = path;
    append_path(path_, "vid_");

    /* std::put_time() isn't supported until gcc 5.0 using the strftime now. */
    /* todo: localtime() is mt_unsafe */
    if (0 == strftime(fmt_time_str, sizeof(fmt_time_str), "%Y_%m_%d_%H_%M_%S",
                      std::localtime(&t))) {
        rc = EIO;
        goto bail;
    }
    //std::stringstream file_name;
    //file_name << std::put_time(std::localtime(&t), "%Y_%m_%d_%H_%M_%S");
    //path_.append(file_name.str());
    path_.append(fmt_time_str);
    pos = path_.length();

    for (int n = 1; EEXIST == (rc = create_h264_file(path_)); n++) {

        path_.replace(pos, path_.length(), std::to_string(n));
    }

    if (0 != rc) {
        goto bail;
    }

    sink_ = std::make_shared<OmxFileSink>();
    if (0 == sink_.use_count()) {
        rc = ENOMEM;
        goto bail;
    }

bail:
    return rc;
}

