#include <stdio.h>
#include <string.h>

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
		return atol(p);
	}
	return -1;
}


int main(void)
{
	FILE *fp = fopen("/proc/diskstats", "r");

	if (!fp)
		return -1;
	printf("sectors read: %ld\n", get_sectors_read(fp, 25));
	
	return 0;
}
