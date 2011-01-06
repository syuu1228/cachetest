#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "common.h"

static const long nr_regions = 160;
static size_t page_size;
static unsigned char *mincore_vec;

struct access_log {
	int nreq;
	long long unsigned hit;
	long sectors_read;
};

static struct access_log global_log = {0,};
static struct access_log obj_log[NUM_OBJ] = {{0,}};

static long get_sectors_read(FILE *fp, int disk)
{
	char buf[255], *p;
	int i;
	
	while(fgets(buf, 255, fp)) {
		if(disk-- > 0)
			continue;
		p = strtok(buf, " ");
		if (!p) {
			fprintf(stderr, "strtok failed\n");
			return -1;
		}
		for(i = 2; i <= 6; i++) {
			p = strtok(NULL, " ");
			if (!p) {
				fprintf(stderr, "strtok failed\n");
				return -1;
			}
		}
		rewind(fp);
		return atol(p);
	}
	return -1;
}

static size_t fincore(int fd)
{
	void *file_mmap;
	size_t page_index;
	size_t cached = 0;

	file_mmap = mmap((void *)0, OBJ_SIZE, PROT_NONE, MAP_SHARED, fd, 0 );
	if ( file_mmap == MAP_FAILED ) {
		perror( "mmap failed" );
		goto cleanup;      
	}

	if ( mincore(file_mmap, OBJ_SIZE, mincore_vec) != 0 ) {
		perror( "mincore" );
		exit( 1 );
	}

	for (page_index = 0; page_index <= OBJ_SIZE/page_size; page_index++)
		if (mincore_vec[page_index]&1)
			++cached;

cleanup:
	if ( file_mmap != MAP_FAILED )
		munmap(file_mmap, OBJ_SIZE);

	return (size_t)((long)cached * (long)page_size);
}

static void print_access_log(struct access_log *log)
{
	long long unsigned received = log->nreq * (long long unsigned)OBJ_SIZE;
	printf("req:%d hit:%llu/%llu %f%% sectors_read:%ld\n",
		   log->nreq, log->hit, received,
		   ((double)log->hit/(double)received)*100,
		   log->sectors_read);
}

int main(int argc, char **argv)
{
	int bind_fd;
	unsigned optval = 1;
	struct sockaddr_in bind_addr;
	FILE *diskstats;	
	int disk;

	if (argc < 2) {
		fprintf(stderr, "more arguments required\n");
		return -1;
	}
		
	disk = atoi(argv[1]);
	if (disk < 0) {
		fprintf(stderr, "bad argument\n");
		return -1;
	}
	
	diskstats = fopen("/proc/diskstats", "r");
	if (!diskstats)
		return -1;
	
	page_size = getpagesize();
	mincore_vec = calloc(1, (OBJ_SIZE+page_size-1)/page_size);
	if ( mincore_vec == NULL ) {
		perror( "Could not calloc" );
		exit( 1 );
	}
	
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
		snprintf(obj_name, sizeof(obj_name), "obj_%d", p.p_seq);
		switch(p.p_type) {
		case P_TYPE_GET: {
			off_t off = 0;
			unsigned long long cached;
			long sec_begin, sec_end;

			sec_begin = get_sectors_read(diskstats, disk);
			if (sec_begin < 0) {
				close(bind_fd);
				close(client_fd);
				return -1;
			}

			obj_fd = open(obj_name, O_RDONLY);
			if (obj_fd < 0) {
				perror("open");
				close(bind_fd);
				close(client_fd);
				return obj_fd;
			}
			
			cached = fincore(obj_fd);
			global_log.nreq++;
			global_log.hit += cached;
			obj_log[p.p_seq].nreq++;
			obj_log[p.p_seq].hit += cached;

			if((siz = sendfile(client_fd, obj_fd, &off, OBJ_SIZE))
			   != OBJ_SIZE) {
				perror("sendfile");
				close(obj_fd);
				close(bind_fd);
				close(client_fd);
				return -1;
			}
			
			close(obj_fd);
			
			sec_end = get_sectors_read(diskstats, disk);
			if (sec_begin < 0) {
				close(obj_fd);
				close(bind_fd);
				close(client_fd);
				return -1;
			}

			global_log.sectors_read += sec_end - sec_begin;
			obj_log[p.p_seq].sectors_read += sec_end - sec_begin;
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
			obj = mmap(NULL, OBJ_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,
					   obj_fd, 0);
			if (obj == MAP_FAILED) {
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
		case P_TYPE_DUMP: {
			int i, j, max = 0;
			for (i = 0; i < NUM_OBJ; i++) {
				for(j = 0; j < NUM_OBJ; j++) {
					if (obj_log[max].nreq < obj_log[j].nreq)
						max = j;
				}
				printf("%d ", max);
				print_access_log(&obj_log[max]);
				obj_log[max].nreq = 0;
				obj_log[max].hit = 0;
			}
			printf("\nglobal:\n");
			print_access_log(&global_log);
			break;
		}
		default:
			fprintf(stderr, "illigual type\n");
		}
		close(client_fd);
	}

	close(bind_fd);
	if ( mincore_vec != NULL ) {
		free(mincore_vec);
		mincore_vec = NULL;
	}
	
	
	return 0;
}
