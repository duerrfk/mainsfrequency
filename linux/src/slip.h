#ifndef SLIP_H
#define SLIP_H

#include <sys/types.h>

ssize_t slip_recvpkt(int fd, void *pktbuffer, size_t pktbuffer_size);

#endif
