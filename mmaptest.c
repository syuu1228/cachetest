#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

int main(void)
{
	struct stat st;
	int fd  = open("tmp.obj", O_RDONLY);
	if (fd < 0) {
		perror("open");
		return fd;
	}
	if(fstat(fd, &st)) {
		perror("fstat");
		return -1;
	}
	void *p = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED | MAP_LOCKED, fd, 0);
	if (p == MAP_FAILED) {
		perror("mmap");
		return -1;
	}
	if(munmap(p, st.st_size)) {
		perror("munmap");
		return -1;
	}
	close(fd);
	
}
