/*********************************************************************
 *                     openNetVM
 *       https://github.com/sdnfv/openNetVM
 *
 *  Copyright 2015 George Washington University
 *            2015 University of California Riverside
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * msgbuf.h - buffer, send and receive the packets to/from controller
 ********************************************************************/

#ifndef MSGBUF_H
#define MSGBUF_H

struct msgbuf 
{
	char* buf;
	int len, start, end;
};

struct msgbuf* msgbuf_new(int bufsize);
int msgbuf_read(struct msgbuf *mbuf, int sock);
int msgbuf_read_all(struct msgbuf *mbuf, int sock, int len);
int msgbuf_write(struct msgbuf *mbuf, int sock, int len);
int msgbuf_write_all(struct msgbuf *mbuf, int sock, int len);
void msgbuf_grow(struct msgbuf *mbuf);
void msgbuf_clear(struct msgbuf *mbuf);
void* msgbuf_peek(struct msgbuf *mbuf);
int msgbuf_pull(struct msgbuf *mbuf, char *buf, int count);
void msgbuf_push(struct msgbuf *mbuf, char *buf, int count);
#define msgbuf_count_buffered(mbuf) ((mbuf->end - mbuf->start))

#endif
