#ifndef _COMMON_H_
#define _COMMON_H_
#include <stdint.h>
#include <sys/sendfile.h>
#define __USE_GNU
#include <fcntl.h>
#undef __USE_GNU
#include <errno.h>
#include <poll.h>

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

static inline int send_direct(int client_fd, int obj_fd, size_t len)
{
	static off_t off = 0;

	return sendfile(client_fd, obj_fd, &off, len);
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

extern int pipefd[2];

static inline int recv_direct(int client_fd, int obj_fd, size_t len)
{
	ssize_t remaining = len;

	while (remaining > 0) {
		struct pollfd pfd;
		ssize_t siz;

		pfd.fd = client_fd;
		pfd.events = POLLIN;
		if (poll(&pfd, 1, -1) != 1)
			continue;


		if ((siz = splice(client_fd, NULL, pipefd[1], NULL, remaining,
						   SPLICE_F_NONBLOCK | SPLICE_F_MOVE)) < 0)
			return siz;
		
		if (!siz)
			break;
		
		if ((siz = splice(pipefd[0], NULL, obj_fd, NULL, siz,
						  SPLICE_F_MORE | SPLICE_F_MOVE)) < 0)
			return siz;
	}

	return len;
}

#endif
