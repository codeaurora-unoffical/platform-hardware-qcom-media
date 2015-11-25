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
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <syslog.h>
#include <cstring>
#include <csignal>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "camerad.h"
#include "qcamvid_log.h"
#include "qcamdaemon.h"
#include "pid_lock.h"
#include <array>
#include "camerad.json.h"
#include <fstream>
#include "camerad_util.h"

using namespace camerad;
using namespace std;

bool STDERR_LOGGING = false;
int  QCAM_LOG_LEVEL = QCAM_LOG_INFO;

/*******************************************************************************
 * Home directory for the daemon.
 ******************************************************************************/
static const char* home_dir = "/";

const char usageStr[] =
    "A video daemon to preview and capture H264 video from camera\n"
    "\n"
    "usage: camerad [options]\n"
    "\n"
    "  -l              enable logging on stderr\n"
    "  -q              kill server\n"
    "  -d <level>      logging level [2]\n"
    "                    0: silent\n"
    "                    1: error\n"
    "                    2: info\n"
    "                    3: debug\n"
    "  -D              server working directory\n"
    "                    all other paths will be relative to this folder.\n"
    "                    default \"/\"\n"
    "  -h              print this message\n"
    "  -p              run as foreground program\n"
;

static inline void printUsageExit()
{
    QCAM_MSG("%s", usageStr);
    exit(0);
}

/* Validates the app configuration. If invalid, it
   shows error and exits the program. */
static void validateConfigExit(DaemonConfig& cfg)
{
    if (cfg.logLevel < 0 || cfg.logLevel >= QCAM_LOG_MAX) {
        QCAM_MSG("ERROR: Invalid log level\n");
        printUsageExit();
    }
}

/* parses commandline options and populates the config
   data structure */
static DaemonConfig parseCommandline(int argc, char* argv[])
{
    DaemonConfig cfg;
    int c;

    while ((c = getopt(argc, argv, "phlqd:D:")) != -1) {
        switch (c) {
        case 'l':
            STDERR_LOGGING = true;
            break;
        case 'q':
            cfg.quit = true;
            break;
        case 'd':
            cfg.logLevel = (DaemonLoglevel) atoi(optarg);
            QCAM_LOG_LEVEL = cfg.logLevel;
            break;
        case 'p':
            cfg.daemon = false;
            break;
        case 'D':
            home_dir = (const char*)optarg;
            break;
        case 'h':
        case '?':
            printUsageExit();
        default:
            abort();
        }
    }
    return cfg;
}

/**
 process a config_file and load in to memory.

 This is a singleton object lazy loaded to memory on the first get()
 **/
static
class Cfg {
    char* cfg_ = NULL;
    uint32_t siz_ = 0;

public:
    virtual ~Cfg() { delete[] cfg_; }
    int get(JSONParser& jp) {
        if (NULL == cfg_) {  /* open and read the config file */
            std::ifstream fs(CONFIG_FILE, std::ios::in | std::ios::binary | std::ios::ate);

            if (fs.is_open()) {
                siz_ = static_cast<uint32_t>(fs.tellg());
                cfg_ = new char[siz_];
                if (NULL != cfg_) {
                    fs.seekg(0, std::ios::beg);
                    fs.read(cfg_, siz_);
                }
                fs.close();
            }
            else {  /* use the default config */
                siz_ = strlen(default_config);
                cfg_ = new char[siz_];

                if (NULL != cfg_) {
                    memmove(cfg_, default_config, siz_);
                }
            }
        }

        if (NULL == cfg_) {
            return EIO;
        }
        JSONParser_Ctor(&jp, cfg_, (int)siz_);
        return 0;
    }
}cfg_;

/**
 get the configuration string corresponding to the camera identified by indx.

 @param indx
 @param jpret
 @return int : 0 on when succesfully retrieved the configuration.
 **/
int camerad::cfgGetCamera(int indx, JSONParser& jpret)
{
    JSONID jsid;
    JSONEnumState jsenum;
    int rc = 0;
    JSONParser jp;

    TRY(rc, cfg_.get(jp));

    rc = ENODEV;

    if (JSONPARSER_SUCCESS == JSONParser_Lookup(&jp, 0, "cameras", 0, &jsid)
        && JSONPARSER_SUCCESS == JSONParser_ArrayEnumInit(&jp, jsid, &jsenum)) {
        JSONID jsid_camera;
        unsigned int id;

        /* for each camera try to match the id */
        while (JSONPARSER_SUCCESS == JSONParser_ArrayNext(&jp, &jsenum,
                                                          &jsid_camera)) {
            if (JSONPARSER_SUCCESS == JSONParser_Lookup(
                &jp, jsid_camera, "id", 0, &jsid) &&
                JSONPARSER_SUCCESS == JSONParser_GetUInt(&jp, jsid, &id)
                && (unsigned int)indx == id) {  /* found the camera of interest */
                const char* p;
                int n;

                JSONParser_GetJSON(&jp, jsid_camera, &p, &n);
                JSONParser_Ctor(&jpret, p, n);

                rc = 0;
                break;
            }
        }
    }

    CATCH(rc) {}
    return rc;
}


/* prints application config on stderr */
static
void printConfig(const DaemonConfig &cfg)
{
    QCAM_MSG("==== camerad configuration ====\n");
    QCAM_MSG("Home Dir = %s\n", home_dir);
    QCAM_MSG("log level = %d\n", cfg.logLevel);
    QCAM_MSG("quit? = %s\n", cfg.quit ? "yes" : "no");
    QCAM_MSG("daemon? = %s\n", cfg.daemon ? "yes" : "no");
    QCAM_MSG("===============================\n");
}

/*******************************************************************************
 * capture the value of terminate signal, for a graceful termination.
 *
 * @param
 ******************************************************************************/
volatile sig_atomic_t g_received_sigterm = 0;
static void sigterm_handler(int sig)
{
	g_received_sigterm = sig;
    QCAM_INFO("%s:%s[%d]", __FILE__, __func__, __LINE__);
}

/*******************************************************************************
 * create a true daemon with no controlling tty. set the working directory
 * and file-system mask.
 * Has similarity to bsd daemon(), but conforming to POSIX
 *
 * @param path : set current working directory to this path
 * @param mask : change the file creation mask
 * @param signal_handler : signal handler for graceful termination. This blocks
 *  the signals until the future call to pselect(). NULL if not interested in the
 *  support.
 * @param debug_mode : a convinience for debugging. This will leave the stdout to
 *   the parent terminal.
 *
 * @return int : On successful return from this subroutine is in the
 * daemon-ized process.
 ******************************************************************************/
int a_daemon(const char* path, mode_t mask, sighandler_t signal_handler,
             bool debug_mode = false)
{
    pid_t daemon_pid;

    daemon_pid = fork();
    if (-1 == daemon_pid) {
        return errno;
    }
    if (0 < daemon_pid) {
        exit(EXIT_SUCCESS); /* orphan the daemon */
    }

    if (false == debug_mode) {
        daemon_pid = setsid(); /* new session leader */
        if (fork()) exit(EXIT_SUCCESS);  /* eliminate the session leader */

        /* close all descriptors */
        for (int i = sysconf(_SC_OPEN_MAX); 0 <= i; --i) close(i);
        /* re-direct standard I/O/E to /dev/null */
        dup(dup(open("/dev/null",O_RDWR)));
    }
    umask(mask);

    int rc = chdir(path);

    if (0 == rc) {
        /* auto-reap zombies */
        signal(SIGCHLD, SIG_IGN);

        if (NULL != signal_handler) {
            /* handle some signals, but first block them until the daemon
               is ready on pselect() */
            std::array<int, 3> handled_sigs = {SIGHUP, SIGTERM, SIGQUIT};
            sigset_t blockset;

            sigemptyset(&blockset);

            for (auto i = handled_sigs.begin(); i != handled_sigs.end(); ++i) {
                sigaddset(&blockset, *i);
                signal(*i, signal_handler);
            }
            sigprocmask(SIG_BLOCK, &blockset, NULL);
        }
    }

    return rc;
}

int main(int argc, char* argv[])
{
    int rc = 0;
    int pid_lock = -1;
    int pid;
    DaemonConfig cfg = parseCommandline(argc, argv);

    validateConfigExit(cfg);
    printConfig(cfg);

    /* Are we just stopping the daemon? */
    if (cfg.quit) {
        /* make sure the server exists */
        rc = pidfile_acquire(PID_FILE, &pid, &pid_lock);
        if (EEXIST != rc) {
            QCAM_ERR("Server not found, probed : %s", PID_FILE);
            rc = ESRCH;
        }
        else {
            rc = kill(pid, SIGTERM);
            if (-1 == rc) {
                rc = errno;
            }
        }
        goto bail;
    }

    /* Shall we transform in to daemon process? */
    if (!rc && cfg.daemon) {
        /* run as daemon. on a successful return the run-time will be in
           daemon process */
        rc = a_daemon(home_dir, S_IWGRP|S_IWOTH, sigterm_handler, STDERR_LOGGING);
    }

    if (!rc) {
        /* make sure there is atmost one instance of qcamdaemon */
        rc = pidfile_acquire(PID_FILE, &pid, &pid_lock);
        if (EEXIST == rc) {
            QCAM_ERR("pid file (%s) locked by %d", PID_FILE, pid);
        }
    }

    if (!rc) {
        /* now run the actual program. this must use pselect() or equivalent
           to turn on signals */
        rc = daemonMain(cfg);
    }

bail:
    pidfile_release(PID_FILE, &pid_lock);
    return rc;
}

