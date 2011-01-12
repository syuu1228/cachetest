#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

int main(int argc, char **argv)
{
	char *mem_pool;
	int fd;
	struct sockaddr_in addr;

	if (argc < 3) {
		fprintf(stderr, "argument required\n");
		return -1;
	}
	
	if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
		perror("mlockall");
		return -1;
	}
	
	mem_pool = (char *)malloc(POOL_SIZE);
	if (!mem_pool) {
		perror("malloc");
		return -1;
	}
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	
	if (!strcmp(argv[2], "put")) {
		if (argc < 4) {
			fprintf(stderr, "argument required\n");
			return -1;
		}
		
		FILE *fp = fopen(argv[3], "r");
		if (!fp) {
			perror("fopen");
			return -1;
		}

		fd = open("/dev/sda", O_RDONLY);
		if (fd < 0) {
			perror("open");
			return fd;
		}
	
		if((read(fd, (void *)mem_pool, POOL_SIZE)) < POOL_SIZE) {
			perror("read");
			close(fd);
			return -1;
		}
	
		close(fd);

		for (;;) {
			struct packet p;
			ssize_t siz;
			char *obj;
			char buf[256];

			if (!fgets(buf, sizeof(buf), fp))
				break;

			p.p_type = P_TYPE_PUT;
			p.p_seq = atoi(buf);
			obj = mem_pool + (OBJ_SIZE * p.p_seq);

			if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				perror("socket");
				return -1;
			}

			if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
				perror("connect");
				close(fd);
				return -1;
			}

			if ((siz = send(fd, &p, sizeof(struct packet), MSG_MORE))
				!= sizeof(struct packet)) {
				perror("send");
				close(fd);
				return -1;
			}
		
			if (send_obj(fd, obj, (size_t)OBJ_SIZE, 0)) {
				perror("send");
				close(fd);
				return -1;
			}

			close(fd);
		}
		
		fclose(fp);
	}else if (!strcmp(argv[2], "get")) {
		if (argc < 4) {
			fprintf(stderr, "argument required\n");
			return -1;
		}
		
		FILE *fp = fopen(argv[3], "r");
		if (!fp) {
			perror("fopen");
			return -1;
		}

		for(;;) {
			struct packet p;
			ssize_t siz;
			char *obj;
			char buf[OBJ_SIZE];

			if (!fgets(buf, OBJ_SIZE, fp))
				break;
			
			p.p_type = P_TYPE_GET;
			p.p_seq = atoi(buf);
			obj = mem_pool + (OBJ_SIZE * p.p_seq);

			if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				perror("socket");
				return -1;
			}
		
			if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
				perror("connect");
				close(fd);
				return -1;
			}
		
			if ((siz = send(fd, &p, sizeof(struct packet), 0))
				!= sizeof(struct packet)) {
				perror("send");
				close(fd);
				return -1;
			}
		
			if (recv_obj(fd, buf, OBJ_SIZE, 0)) {
				perror("recv");
				close(fd);
				return -1;
			}
		
			close(fd);
		}
		
		fclose(fp);
	}else if (!strcmp(argv[2], "dump")) {
		struct packet p;
		ssize_t siz;
		
		p.p_type = P_TYPE_DUMP;
		
		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			perror("socket");
			return -1;
		}
		
		
		if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
			perror("connect");
			close(fd);
			return -1;
		}
		
		if ((siz = send(fd, &p, sizeof(struct packet), 0))
			!= sizeof(struct packet)) {
			perror("send");
			close(fd);
			return -1;
		}
		close(fd);
	}
	return 0;
}
