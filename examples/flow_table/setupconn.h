#ifndef SETUPCONN_H
#define SETUPCONN_H

#ifndef BUFLEN
#define BUFLEN 65536
#endif


int  timeout_connect(int fd, const char *hostname, int port, int mstimeout);
int  make_tcp_connection(const char *hostname, unsigned short port);

#endif
