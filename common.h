#ifndef _COMMON_H_
#define _COMMON_H_
#include <stdint.h>
#include <sys/sendfile.h>
#define __USE_GNU
#include <fcntl.h>
#undef __USE_GNU
#include <errno.h>

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
		printf("send:%Zd left:%zd\n", ret, len);
	} while(len);
	printf("return 0\n");
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


static void test_splice(int in, int out, int number_of_bytes) {
    int rcvd = 0, sent = 0, teed = 0, remaining = number_of_bytes;
    int pipe1[2];

    if (pipe(pipe1) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    while (remaining > 0) {
        if ((rcvd = splice(in, NULL, pipe1[1], NULL, remaining,
                SPLICE_F_MORE | SPLICE_F_MOVE)) < 0) {
            perror("splice");
            exit(EXIT_FAILURE);
        }

        if (rcvd == 0) {
            printf("Reached end of input file\n");
            break;
        }

        printf("Wrote %d bytes to pipe1\n", rcvd);

        if ((sent = splice(pipe1[0], NULL, out, NULL, rcvd, SPLICE_F_MORE
                | SPLICE_F_MOVE)) < 0) {
            perror("splice");
            exit(EXIT_FAILURE);
        }

        printf("Read %d bytes from pipe1\n", sent);
        remaining -= rcvd;
    }
}


static inline ssize_t do_recvfile(int out_fd, int in_fd, off_t offset, size_t count) {
    ssize_t bytes, bytes_sent, bytes_in_pipe;
    size_t total_bytes_sent = 0;

    // Splice the data from in_fd into the pipe
    while (total_bytes_sent < count) {
      printf("data -> pipe\n");
        if ((bytes_sent = splice(in_fd, NULL, pipefd[1], NULL,
                count - total_bytes_sent,
                SPLICE_F_MORE | SPLICE_F_MOVE)) <= 0) {
            if (errno == EINTR || errno == EAGAIN) {
		printf("EINTR || EAGAIN\n");
                // Interrupted system call/try again
                // Just skip to the top of the loop and try again
                continue;
            }
            perror("splice");
            return -1;
        }
	printf("bytes_sent:%Zd\n", bytes_sent);
        // Splice the data from the pipe into out_fd
        bytes_in_pipe = bytes_sent;
        while (bytes_in_pipe > 0) {
            printf("pipe -> data\n");
            if ((bytes = splice(pipefd[0], NULL, out_fd, &offset, bytes_in_pipe,
                    SPLICE_F_MORE | SPLICE_F_MOVE)) <= 0) {
                if (errno == EINTR || errno == EAGAIN) {
                    printf("EINTR || EAGAIN\n");
                    // Interrupted system call/try again
                    // Just skip to the top of the loop and try again
                    continue;
                }
                perror("splice");
                return -1;
            }
            printf("bytes:%Zd\n", bytes);
            bytes_in_pipe -= bytes;
        }
        total_bytes_sent += bytes_sent;
    }
    printf("return %Zd\n", total_bytes_sent);
    return total_bytes_sent;
}
static inline int recv_direct(int client_fd, int obj_fd, size_t len)
{
  //	return do_recvfile(obj_fd, client_fd, 0, len);
  test_splice(client_fd, obj_fd, len);
  return len;
}

/*
static inline int recv_direct(int client_fd, int obj_fd, size_t len)
{
	int ret = 0;
	ssize_t written, siz;
	int pipedes[2];
	
	if(pipe(pipedes) < 0) {
		puts("pipe error");
		return -1;
	}

	int pipe_r = pipedes[0];
	int pipe_w = pipedes[1];

	for (written = 0; written < len; written += siz) {
		loff_t off = (loff_t)written;
		siz = splice(client_fd, &off, pipe_w, NULL,
			len - written, SPLICE_F_MORE | SPLICE_F_MOVE);
		printf("written:%Zd siz:%Zd\n", written, siz);
		if (siz < 0) {
			puts("splice error");
			ret = siz;
			goto out;
		}
	}

	for (written = 0; written < len; written += siz) {
		loff_t off = (loff_t)written;
		siz = splice(pipe_r, NULL, obj_fd, &off,
			len - written, SPLICE_F_MORE | SPLICE_F_MOVE);
		printf("written:%Zd siz:%Zd\n", written, siz);
		if (siz < 0) {
			puts("splice2 error");
			ret = siz;
			goto out;
		}
	}
out:
	close(pipe_r);
	close(pipe_w);
	return ret;
}
*/
#endif
