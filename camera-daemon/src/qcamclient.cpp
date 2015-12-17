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
#include "qcamclient.h"
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define PROMPT_STR "> "
#define PUT_PROMPT() fputs(PROMPT_STR, stdout), fflush(stdout)
/** readline to buffer "b" with size "s"  */
#define READLINE(b, s) (PUT_PROMPT(), NULL != fgets(b, s, stdin))

namespace camvid {

static
int connectToDaemon(void)
{
    int sock = -1;
    int ret, val = 1;
    struct sockaddr_in dameonaddr;

    dameonaddr.sin_family = AF_INET;
    dameonaddr.sin_addr.s_addr = inet_addr("127.0.0.1");  /* TODO: support command parameter */
    dameonaddr.sin_port = htons(PORT);

    /* Create the socket. */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ret = -errno;
        goto bail;
    }

    ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
    if (ret < 0) {
        ret = -errno;
        goto bail;
    }

    ret = connect(sock, (struct sockaddr *)&dameonaddr, sizeof(dameonaddr));
    if (ret < 0) {
        ret = -errno;
        goto bail;
    }

    return sock;

  bail:
    if (0 < sock) {
        close(sock);
    }
    return ret;
}

int Client::send(char* buf, int len)
{
  int bytes_written;

  bytes_written = write(sock_, buf, len);
  if (bytes_written < 0) {
      fprintf(stderr, "write : %s\n", strerror(errno));
      return -1;
  }
  return bytes_written;
}

void Client::batch_process(void)
{
    /* todo: batch process from the file */
}

void Client::reader(void)
{
    char buf[1024];

    while (!stop_) {
        struct timeval ts;
        int n;

        // 1 second
        ts.tv_sec = 1;
        ts.tv_usec = 0;
        setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, (char*)&ts,
                   sizeof(struct timeval));

        // wait for data
        n = read(sock_, buf, sizeof(buf)-1);

        if (0 < n) {
            buf[n] = 0;
            fputs("RECV : ", stdout);
            if(puts(buf) == EOF) {
                fprintf(stderr, "fputs : %s", strerror(errno));
            }
            PUT_PROMPT();
        }
    }
}

int Client::start(void)
{
    std::lock_guard<std::mutex> guard(lock_);
    if (th_.joinable()) {
        return EALREADY;
    }

    sock_ = connectToDaemon();
    if (sock_ < 0) {
        fprintf(stderr, "Bad Socket : %s\n", strerror(-sock_));
        return errno;
    }

    stop_ = false;
    th_ = std::thread(&Client::reader, this);

    return 0;
}

void Client::stop(void)
{
    std::lock_guard<std::mutex> guard(lock_);

    if (th_.joinable()) {
        stop_ = true;
        th_.join();
    }

    if (-1 < sock_) { close(sock_); sock_ = -1; }
}

int Client::create(const CmdConfig& cfg, ClientPtr* pa)
{
    ClientPtr a = make_shared();

    *pa = a;

    a->cfg_ = cfg;

    return 0;
}

}

static const char usageStr[] =
    "front end app for camera daemon\n"
    "\n"
    "usage: camclient [options]\n"
    "\n"
    "  -i              interactive shell\n"
    "  -b <file>       batch mode operation\n"
    "  -h              print this message\n"
;

static inline void printUsageExit()
{
    fprintf(stdout, "%s", usageStr);
    exit(0);
}

/* parses commandline options and populates the config
   data structure */
static camvid::CmdConfig getConfig(int argc, char* argv[])
{
    camvid::CmdConfig cfg;
    int c;

    if (argc < 2) {
        printUsageExit();
    }

    while ((c = getopt(argc, argv, "hib:")) != -1) {
        switch (c) {
        case 'i':
            cfg.interactive = true;
            break;
        case 'b':
            cfg.input_file = optarg;
            break;
        case 'h':
        case '?':
        default:
            printUsageExit();
        }
    }
    return cfg;
}

static int cmd_sleep(camvid::ClientPtr& inst, char* arg)
{
    unsigned int sleep_secs = strtol(arg, NULL, 10);

    if (sleep_secs) {
        sleep(sleep_secs);
    }

    return 0;
}

static int cmd_help(camvid::ClientPtr& inst, char* arg);

static int cmd_quit(camvid::ClientPtr& inst, char* arg)
{
    return -1;
}

static int cmd_send(camvid::ClientPtr& inst, char* arg)
{
    return inst->send(arg, strlen(arg));
}

typedef struct {
    const char* cmd;
    int (*cmd_handler)(camvid::ClientPtr&, char*);
    const char* help;
} cmd_t;

#define ARRSIZ(a) ((int)((sizeof((a))/sizeof((a)[0]))))
#define CMD(func, help) {#func, cmd_ ## func, help}
#define CMD_LINE_SIZE 512

// supported table
static cmd_t cmds[] = {
    CMD(quit,"Exits the interpreter"),
    CMD(help,"Display this help"),
    CMD(send,"Send a jsonrpc message"),
    CMD(sleep,"pause the interpretor for number of seconds"),
};

inline bool isSpace(char c)         { return c == ' ' ||  c == '\t' || c == '\r' || c == '\n'; }
inline void skipSpace(char*& s)     { while(*s && isSpace(*s)) s++; }
inline void skipUptoSpace(char*& s) { while(*s && !isSpace(*s)) s++; }

static char* cmd_split(char* s, char** tail)
{

    /* skip leading space */
    skipSpace(s);

    if (0 == *s) {
        *tail = NULL;
        return NULL;
    }

    char* head = s;

    skipUptoSpace(s);

    if (0 == *s) {
        *tail = NULL;
    }
    else {
        *s++ = '\0';  /* zero terminate the head */

        skipSpace(s);
        *tail = (0 == *s) ? NULL : s;
    }

    return head;
}

static int cmd_help(camvid::ClientPtr& inst, char* arg)
{
    for (int i = 0; i < ARRSIZ(cmds); i++) {
        fputs(cmds[i].cmd, stdout);
        fputs("\t", stdout);
        puts(cmds[i].help);
    }

    return 0;
}

static int cmd_dispatch(camvid::ClientPtr& inst, const char* cmd, char* arg)
{
    if (cmd) {
        for (int i = 0; i < ARRSIZ(cmds); i++) {
            if (0 == strcmp(cmd, cmds[i].cmd)) {
                return cmds[i].cmd_handler(inst, arg);
            }
        }
    }

    puts("ERR: Command not found");

    return -1;
}

int main(int argc, char* argv[])
{
    int rc = 0;
    char cmd_line[CMD_LINE_SIZE];
    camvid::ClientPtr client;
    camvid::CmdConfig cfg = getConfig(argc, argv);

    camvid::Client::create(cfg, &client);

    rc = client->start();
    if (0 == rc && !cfg.interactive) {
        /* batch process */
        if (NULL == freopen(cfg.input_file, "r", stdin)) {
            rc = errno;
            fprintf(stderr, "open file(%s) : %s\n", cfg.input_file, strerror(rc));
        }
    }

    while(0 == rc && 0 != READLINE(cmd_line, sizeof(cmd_line))) {
        const char* cmd;
        char* arg;

        cmd = cmd_split(cmd_line, &arg);
        if (cmd) {
            if (cmd_dispatch(client, cmd, arg) < 0) {
                break;
            }
        }
    }

    client->stop();
    puts("");

    return rc;
}
