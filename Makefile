OBJS = fadvcache kernelcache mlockcache
CFLAGS = -Wall -Werror -g
all: $(OBJS)
kernelcache:
	$(CC) $(CFLAGS) -o kernelcache kernelcache.c
fadvcache:
	$(CC) $(CFLAGS) -o fadvcache kernelcache.c -DFADVCACHE
mlockcache:
	$(CC) $(CFLAGS) -o mlockcache kernelcache.c -DMLOCKCACHE
clean:
	rm -fv $(OBJS)
