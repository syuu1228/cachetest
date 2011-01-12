OBJS = fadvcache kernelcache mlockcache mlockcache_keep_put
CFLAGS = -Wall -Werror -g
all: $(OBJS)
fadvcache:
	$(CC) $(CFLAGS) -o fadvcache kernelcache.c -DFADVCACHE
mlockcache:
	$(CC) $(CFLAGS) -o mlockcache kernelcache.c -DMLOCKCACHE
mlockcache_keep_put:
	$(CC) $(CFLAGS) -o mlockcache_keep_put kernelcache.c -DMLOCKCACHE -DMLOCKCACHE_KEEP_PUT
clean:
	rm -fv $(OBJS)
