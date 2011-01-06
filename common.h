#ifndef _COMMON_H_
#define _COMMON_H_
#include <stdint.h>

#define OBJ_SIZE (1024 * 1024) /* 1MB */
#define NUM_OBJ (1024)
#define POOL_SIZE (OBJ_SIZE * NUM_OBJ) /* 1GB */
#define SERVER_PORT (10000)

#define P_TYPE_GET (0)
#define P_TYPE_PUT (1)
#define P_TYPE_DUMP (2)

struct packet {
	uint32_t p_type;
    uint32_t p_seq;
};

static inline int send_obj(int fd, char *p, size_t len, int flags)
{
	ssize_t ret;
	do {
		if((ret = send(fd, p, len, flags)) < 0)
			return ret;
		len -= ret;
		p += ret;
	} while(len);
	
	return 0;
}

static inline int recv_obj(int fd, char *p, size_t len, int flags)
{
	ssize_t ret;
	do {
		if((ret = recv(fd, p, len, flags)) < 0)
			return ret;
		len -= ret;
		p += ret;
	} while(len);
	
	return 0;
}

#endif
