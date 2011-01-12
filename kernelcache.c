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
#include <time.h>
#include "common.h"
#if defined(FADVCACHE) || defined(MLOCKCACHE)
#include "list.h"

#define CACHE_MAX (200 * 1000 * 1000)
#endif

static const long nr_regions = 160;
static size_t page_size;
static unsigned char *mincore_vec;

struct access_log {
	char *name;
	int nreq;
	long long unsigned hit;
	long long unsigned last_fincore;
	long sectors_read;
	int dumped;
	time_t last_access;
#if defined(FADVCACHE) || defined(MLOCKCACHE)
	LIST_ENTRY(access_log) list;
	int on_cache;
#endif
#ifdef MLOCKCACHE
	void *obj;
#endif
};

static struct access_log global_log = {0,};
static struct access_log obj_log[NUM_OBJ] = {{0,}};
#if defined(FADVCACHE) || defined(MLOCKCACHE)
static long long unsigned cache_size = 0;
static struct access_log least_access = {0,};
#endif

static long get_sectors_read(FILE *fp, int disk)
{
	char buf[255], *p;
	int i;
	
	while(fgets(buf, 255, fp)) {
		if(disk-- > 0)
			continue;
		p = strtok(buf, " ");
		if (!p) {
			perror("strtok");
			exit(1);
		}
		for(i = 2; i <= 6; i++) {
			p = strtok(NULL, " ");
			if (!p) {
				perror("strtok");
				exit(1);
			}
		}
		rewind(fp);
		return atol(p);
	}
	return -1;
}

#ifdef MLOCKCACHE
static size_t fincore(int fd, void *obj)
#else
static size_t fincore(int fd)
#endif
{
	void *file_mmap;
	size_t page_index;
	size_t cached = 0;

#ifdef MLOCKCACHE
	if (obj)
		file_mmap = obj;
	else
#endif
	{
		file_mmap = mmap(NULL, OBJ_SIZE, PROT_NONE, MAP_SHARED, fd, 0);
		if (file_mmap == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}
	}

	if (mincore(file_mmap, OBJ_SIZE, mincore_vec) != 0) {
		perror("mincore");
		exit(1);
	}

	for (page_index = 0; page_index <= OBJ_SIZE/page_size; page_index++)
		if (mincore_vec[page_index] & 1)
			++cached;

#ifdef MLOCKCACHE
	if (!obj)
#endif
		if (file_mmap != MAP_FAILED)
			munmap(file_mmap, OBJ_SIZE);

	return (size_t)((long)cached * (long)page_size);
}

static void print_header(void)
{
	puts("id, name, req, hit, received, percentage, last_fincore, sectors_read, "
		 "last_access"
#if defined(FADVCACHE) || defined(MLOCKCACHE)
		 ", on_cache"
#endif
		);
}

static void print_access_log(int id, struct access_log *log)
{
	long long unsigned received = log->nreq * (long long unsigned)OBJ_SIZE;
	printf("%d, %s, %d, %llu, %llu, %f, %llu, %ld, %ld"
#if defined(FADVCACHE) || defined(MLOCKCACHE)
		   ", %d"
#endif
		   "\n",
		   id, log->name, log->nreq, log->hit, received,
		   ((double)log->hit/(double)received)*100,
		   log->last_fincore,
		   log->sectors_read,
		   log->last_access
#if defined(FADVCACHE) || defined(MLOCKCACHE)
		   ,log->on_cache
#endif
		);
}

#if defined(FADVCACHE) || defined(MLOCKCACHE)
static void update_least_access(struct access_log *log)
{
	struct access_log *lp;

	if (!LIST_NEXT(&least_access, list)) {
		LIST_INSERT_AFTER(&least_access, log, list);
	}else{
		LIST_FOREACH(lp, log, list) {
			if (LIST_NEXT(lp, list) == NULL ||
				LIST_NEXT(lp, list)->nreq > log->nreq) {
				LIST_REMOVE(log, list);
				LIST_INSERT_AFTER(lp, log, list);
				break;
			}
		}
	}
}

static int cache_mark(int fd, struct access_log *log, int prot, int flag)
{
	struct access_log *lp;

#ifdef MLOCKCACHE
	if (flag == POSIX_FADV_WILLNEED) {
		log->obj = mmap(NULL, OBJ_SIZE, prot, MAP_SHARED | MAP_LOCKED, fd, 0);
		if (log->obj == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}
	}else{
		if (munmap(log->obj, OBJ_SIZE)) {
			perror("munmap");
			exit(1);
		}
		log->obj = NULL;
		if (posix_fadvise(fd, 0, OBJ_SIZE, POSIX_FADV_DONTNEED)) {
			perror("posix_fadvise");
			exit(1);
		}
 	}
#else
	if (posix_fadvise(fd, 0, OBJ_SIZE, flag)) {
		perror("posix_fadvise");
		exit(1);
	}
#endif
	if (flag == POSIX_FADV_WILLNEED) {
		cache_size += OBJ_SIZE;
		log->on_cache = 1;
		if (!LIST_NEXT(&least_access, list)) {
			LIST_INSERT_AFTER(&least_access, log, list);
		}else{
			LIST_FOREACH(lp, &least_access, list) {
				if (!LIST_NEXT(lp, list) ||
					LIST_NEXT(lp, list)->nreq > log->nreq) {
					LIST_INSERT_AFTER(lp, log, list);
					break;
				}
			}
		}
	}

	return 0;
}

static int cache_purge(struct access_log *log)
{
	int fd = open(log->name, O_RDONLY);
#ifdef MLOCKCACHE
	if (munmap(log->obj, OBJ_SIZE)) {
		perror("munmap");
		exit(1);
	}
	log->obj = NULL;
#endif
	if (posix_fadvise(fd, 0, OBJ_SIZE,
					  POSIX_FADV_DONTNEED)) {
		perror("posix_fadvise");
		exit(1);
	}
	cache_size -= OBJ_SIZE;
	log->on_cache = 0;
	LIST_REMOVE(log, list);
	close(fd);
	return 0;
}
#endif

int main(int argc, char **argv)
{
	int bind_fd;
	unsigned optval = 1;
	struct sockaddr_in bind_addr;
	FILE *diskstats;	
	int disk;

	if (argc < 2) {
		fprintf(stderr, "more arguments required\n");
		exit(1);
	}
		
	disk = atoi(argv[1]);
	if (disk < 0) {
		fprintf(stderr, "bad argument\n");
		exit(1);
	}
	
	diskstats = fopen("/proc/diskstats", "r");
	if (!diskstats) {
		perror("fopen");
		exit(1);
	}
	
	page_size = getpagesize();
	mincore_vec = calloc(1, (OBJ_SIZE + page_size -1) / page_size);
	if (mincore_vec == NULL) {
		perror("Could not calloc");
		exit(1);
	}
	
	if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
		perror("mlockall");
		exit(1);
	}

	bind_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (bind_fd < 0) {
		perror("socket");
		exit(1);
	}

	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(SERVER_PORT);
	bind_addr.sin_addr.s_addr = INADDR_ANY;

	if (setsockopt(bind_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) {
		perror("setsockopt");
		exit(1);
	}

	if (bind(bind_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
		perror("bind");
		exit(1);
	}

	if (listen(bind_fd, 1024) != 0) {
		perror("listen");
		exit(1);
	}
	
	while (1) {
		struct sockaddr_in client_addr;
		int client_fd;
		socklen_t client_addr_len = sizeof(struct sockaddr_in);
		int obj_fd;
		struct packet p;
		ssize_t siz;
		char obj_name[100] = {0,};
		struct access_log *log;

		client_fd = accept(bind_fd, (struct sockaddr *)&client_addr,
				   &client_addr_len);
		if (client_fd < 0) {
			perror("accept");
			exit(1);
		}

		if ((siz = recv(client_fd, &p, sizeof(struct packet), 0))
		   != sizeof(struct packet)) {
			perror("recv");
			exit(1);
		}
		snprintf(obj_name, sizeof(obj_name), "obj_%d", p.p_seq);
		log = &obj_log[p.p_seq];
		switch(p.p_type) {
		case P_TYPE_GET: {
			off_t off = 0;
			unsigned long long cached;
			long sec_begin, sec_end;

			log->last_access = time(NULL);
			
			if (!log->name)
				log->name = strdup(obj_name);

			sec_begin = get_sectors_read(diskstats, disk);
			if (sec_begin < 0) {
				perror("sec_begin");
				exit(1);
			}

			obj_fd = open(obj_name, O_RDONLY);
			if (obj_fd < 0) {
				perror("open");
				return obj_fd;
			}

#ifdef MLOCKCACHE
			cached = fincore(obj_fd, log->obj);
#else
			cached = fincore(obj_fd);
#endif
			global_log.nreq++;
			global_log.hit += cached;
			log->nreq++;
			log->hit += cached;
			log->last_fincore = cached;

			if((siz = sendfile(client_fd, obj_fd, &off, OBJ_SIZE))
			   != OBJ_SIZE) {
				perror("sendfile");
				exit(1);
			}

#if defined(FADVCACHE) || defined(MLOCKCACHE)
			if (!log->on_cache) {
				if (log->nreq > 1) {
					if (cache_size + OBJ_SIZE > CACHE_MAX) {
						if (LIST_NEXT(&least_access, list) &&
							LIST_NEXT(&least_access, list)->nreq <= log->nreq) {
							while (LIST_NEXT(&least_access, list) &&
								   cache_size + OBJ_SIZE > CACHE_MAX)
								cache_purge(LIST_NEXT(&least_access, list));
							cache_mark(obj_fd, log, PROT_READ, POSIX_FADV_WILLNEED);
						}
					}else{
						cache_mark(obj_fd, log, PROT_READ, POSIX_FADV_WILLNEED);
					}
				}else{
					cache_mark(obj_fd, log, PROT_READ, POSIX_FADV_DONTNEED);
				}
			}else if(log->on_cache) {
				update_least_access(log);
			}
#endif
			close(obj_fd);
			
			sec_end = get_sectors_read(diskstats, disk);
			if (sec_begin < 0) {
				perror("sec_begin");
				exit(1);
			}

			global_log.sectors_read += sec_end - sec_begin;
			log->sectors_read += sec_end - sec_begin;
			break;
		}
		case P_TYPE_PUT: {
			char *obj;
			
			obj_fd = open(obj_name, O_RDWR|O_CREAT);
			if (obj_fd < 0) {
				perror("open");
				exit(1);
			}
			if (ftruncate(obj_fd, OBJ_SIZE)) {
				perror("ftruncate");
				exit(1);
			}
#ifdef MLOCKCACHE_KEEP_PUT
			if (cache_size + OBJ_SIZE > CACHE_MAX) {
				if (LIST_NEXT(&least_access, list) &&
					LIST_NEXT(&least_access, list)->nreq <= log->nreq) {
					while (LIST_NEXT(&least_access, list) &&
						   cache_size + OBJ_SIZE > CACHE_MAX)
						cache_purge(LIST_NEXT(&least_access, list));
					cache_mark(obj_fd, log, PROT_READ|PROT_WRITE, POSIX_FADV_WILLNEED);
				}
			}else{
				cache_mark(obj_fd, log, PROT_READ|PROT_WRITE, POSIX_FADV_WILLNEED);
			}
			log->last_access = time(NULL);
			
			if (!log->name)
				log->name = strdup(obj_name);
			
			obj = log->obj;
#else
			obj = mmap(NULL, OBJ_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,
					   obj_fd, 0);
#endif
			if (obj == MAP_FAILED) {
				perror("mmap");
				exit(1);
			}
			
			if(recv_obj(client_fd, obj, OBJ_SIZE, 0)) {
				perror("recv");
				exit(1);
			}

#ifndef MLOCKCACHE_KEEP_PUT
			munmap(obj, OBJ_SIZE);
#endif
			close(obj_fd);

			break;
		}
		case P_TYPE_DUMP: {
			print_header();
			for(;;) {
				int i, max = -1;
				for(i = 0; i < NUM_OBJ; i++) {
					if (obj_log[i].dumped)
						continue;
					if (max == -1 || (max != -1 && obj_log[max].nreq < obj_log[i].nreq))
						max = i;
				}
				if (max == -1)
					break;
				print_access_log(max, &obj_log[max]);
				obj_log[max].dumped  = 1;
			}
			printf("\nglobal:\n");
			print_access_log(0, &global_log);
			break;
		}
		default:
			fprintf(stderr, "illigual type\n");
		}
		close(client_fd);
	}
	
	return 0;
}
