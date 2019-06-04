/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * msgbuf.c - buffer, send and receive the packets to/from controller
 ********************************************************************/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "msgbuf.h"

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

struct msgbuf *
msgbuf_new(int bufsize) {
        struct msgbuf *mbuf;
        mbuf = malloc(sizeof(*mbuf));
        assert(mbuf);
        mbuf->len = bufsize;
        mbuf->buf = malloc(mbuf->len);
        assert(mbuf->len);
        mbuf->start = mbuf->end = 0;

        return mbuf;
}

int
msgbuf_read(struct msgbuf *mbuf, int sock) {
        int count = read(sock, &mbuf->buf[mbuf->end], mbuf->len - mbuf->end);
        if (count > 0)
                mbuf->end += count;
        if (mbuf->end >= mbuf->len)  // resize buffer if need be
                msgbuf_grow(mbuf);
        return count;
}

int
msgbuf_read_all(struct msgbuf *mbuf, int sock, int len) {
        int count = 0;
        int tmp;
        while (count < len) {
                tmp = msgbuf_read(mbuf, sock);
                if ((tmp == 0) || ((tmp < 0) && (errno != EWOULDBLOCK) && (errno != EINTR) && (errno != EAGAIN)))
                        return tmp;
                if (tmp > 0)
                        count += tmp;
        }
        return count;
}

int
msgbuf_write(struct msgbuf *mbuf, int sock, int len) {
        int send_len = mbuf->end - mbuf->start;
        if (len > 0) {
                if (send_len < len)
                        return -1;
                if (send_len > len)
                        send_len = len;
        }
        int count = write(sock, &mbuf->buf[mbuf->start], send_len);
        if (count > 0)
                mbuf->start += count;
        if (mbuf->start >= mbuf->end)
                mbuf->start = mbuf->end = 0;
        return count;
}

int
msgbuf_write_all(struct msgbuf *mbuf, int sock, int len) {
        int tmp = 0, count = 0;
        while (mbuf->start < mbuf->end) {
                tmp = msgbuf_write(mbuf, sock, len);
                if ((tmp < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINTR))

                        return tmp;
                if (count > 0)
                        count += tmp;
        }
        return count;
}

void
msgbuf_clear(struct msgbuf *mbuf) {
        mbuf->start = mbuf->end = 0;
}

void
msgbuf_grow(struct msgbuf *mbuf) {
        mbuf->len *= 2;
        mbuf->buf = realloc(mbuf->buf, mbuf->len);
        if (mbuf->buf == NULL) {
                perror("msgbuf_grow failed");
                printf("buffer len: %d\n", mbuf->len);
        }
        assert(mbuf->buf);
}

void *
msgbuf_peek(struct msgbuf *mbuf) {
        if (mbuf->start >= mbuf->end)
                return NULL;
        return (void *)&mbuf->buf[mbuf->start];
}

int
msgbuf_pull(struct msgbuf *mbuf, char *buf, int count) {
        int min = MIN(count, mbuf->end - mbuf->start);
        if (min <= 0)
                return -1;
        if (buf)  // don't write if NULL
                memcpy(buf, &mbuf->buf[mbuf->start], min);
        mbuf->start += min;
        if (mbuf->start >= mbuf->end)
                mbuf->start = mbuf->end = 0;
        return min;
}

void
msgbuf_push(struct msgbuf *mbuf, char *buf, int count) {
        while ((mbuf->end + count) > mbuf->len)
                msgbuf_grow(mbuf);
        memcpy(&mbuf->buf[mbuf->end], buf, count);
        mbuf->end += count;
}
