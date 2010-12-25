#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "common.h"

int main(void)
{
	int bind_fd;
	unsigned optval = 1;
	struct sockaddr_in bind_addr;

	if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
		perror("mlockall");
		return -1;
	}

	bind_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (bind_fd < 0) {
		perror("socket");
		return bind_fd;
	}

	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(SERVER_PORT);
	bind_addr.sin_addr.s_addr = INADDR_ANY;

	if (setsockopt(bind_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) {
		perror("setsockopt");
		return -1;
	}

	if (bind(bind_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
		perror("bind");
		return -1;
	}

	if (listen(bind_fd, 1024) != 0) {
		perror("listen");
		return -1;
	}
	
	while (1) {
		struct sockaddr_in client_addr;
		int client_fd, client_addr_len = sizeof(struct sockaddr_in);
		int obj_fd;
		struct packet p;
		ssize_t siz;
		char obj_name[100] = {0,};

		client_fd = accept(bind_fd, (struct sockaddr *)&client_addr,
				   &client_addr_len);
		if (client_fd < 0) {
			perror("accept");
			close(bind_fd);
			return -1;
		}

		if ((siz = recv(client_fd, &p, sizeof(struct packet), 0))
		   != sizeof(struct packet)) {
			perror("recv");
			close(bind_fd);
			close(client_fd);
			return -1;
		}
		printf("p_type:%d p_seq:%d\n", p.p_type, p.p_seq);
		snprintf(obj_name, sizeof(obj_name), "obj_%d", p.p_seq);
		switch(p.p_type) {
		case P_TYPE_GET: {
			off_t off;
			
			obj_fd = open(obj_name, O_RDONLY);
			if (obj_fd < 0) {
				perror("open");
				return obj_fd;
			}

			off = 0;
			if((siz = sendfile(client_fd, obj_fd, &off, OBJ_SIZE))
			   != OBJ_SIZE) {
				perror("sendfile");
				close(obj_fd);
				close(bind_fd);
				close(client_fd);
				return -1;
			}
			close(obj_fd);
			
			break;
		}
		case P_TYPE_PUT: {
			char *obj;
			
			obj_fd = open(obj_name, O_RDWR|O_CREAT);
			if (obj_fd < 0) {
				perror("open");
				return obj_fd;
			}
			if (ftruncate(obj_fd, OBJ_SIZE)) {
			  perror("ftruncate");
			  close(obj_fd);
			  return -1;
			}
			obj = mmap(NULL, OBJ_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, obj_fd,
					   0);
			if ((int)obj == -1) {
				perror("mmap");
				close(obj_fd);
				close(bind_fd);
				close(client_fd);
				return -1;
			}
			
			if(recv_obj(client_fd, obj, OBJ_SIZE, 0)) {
				perror("recv");
				munmap(obj, OBJ_SIZE);
				close(obj_fd);
				close(bind_fd);
				close(client_fd);
				return -1;
			}
			
			munmap(obj, OBJ_SIZE);
			close(obj_fd);

			break;
		}
		default:
			fprintf(stderr, "illigual type\n");
		}
		close(client_fd);
	}

	close(bind_fd);
	return 0;
}
