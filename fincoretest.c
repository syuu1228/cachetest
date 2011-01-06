#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

long fincore(int fd, size_t length) {
	void *file_mmap;
	size_t page_index;
	size_t cached = 0;
	unsigned char *mincore_vec;
	size_t page_size = getpagesize();
	size_t ret = -1;

	mincore_vec = calloc(1, (length+page_size-1)/page_size);
	if ( mincore_vec == NULL ) {
		perror( "Could not calloc" );
	}

	file_mmap = mmap((void *)0, length, PROT_NONE, MAP_SHARED, fd, 0 );
	if ( file_mmap == MAP_FAILED ) {
		perror( "mmap failed" );
		goto cleanup;
	}

	if ( mincore(file_mmap, length, mincore_vec) != 0 ) {
		perror( "mincore" );
		goto cleanup;
	}

	for (page_index = 0; page_index <= length/page_size; page_index++)
		if (mincore_vec[page_index]&1)
			++cached;
	ret = (size_t)((long)cached * (long)page_size);

cleanup:
	if ( file_mmap != MAP_FAILED )
		munmap(file_mmap, length);
	if ( mincore_vec != NULL )
		free(mincore_vec);

	return ret; 
}

int main(void)
{
	int fd;
	long cached;
	struct stat st;
	
	fd = open("test.dat", O_RDONLY);
	if (fd < 0) {
		perror("open");
		return fd;
	}
	if (fstat(fd, &st)) {
		perror("fstat");
		close(fd);
		return -1;
	}
	cached = fincore(fd, st.st_size);
	printf("%ld/%zd\n", cached, st.st_size);
	close(fd);
	
	return 0;
}
