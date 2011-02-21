OBJS = kernelcache kernelcache_direct fadvcache_lru mlockcache_lru fadvcache_la mlockcache_la client
CFLAGS = -Wall -Werror -g -lproc
all: $(OBJS)
client: client.c
	$(CC) $(CFLAGS) -o $@ $<
kernelcache: kernelcache.c
	$(CC) $(CFLAGS) -o $@ $< -DARGO_LEAST_ACCESS
kernelcache_direct: kernelcache.c recvfile.c
	$(CC) $(CFLAGS) -o $@ $< -DARGO_LRU -DSEND_DIRECT -DRECV_DIRECT
fadvcache_lru: kernelcache.c
	$(CC) $(CFLAGS) -o $@ $< -DFADVCACHE -DARGO_LRU
fadvcache_la: kernelcache.c
	$(CC) $(CFLAGS) -o $@ $< -DFADVCACHE -DARGO_LEAST_ACCESS
mlockcache_lru: kernelcache.c
	$(CC) $(CFLAGS) -o $@ $< -DMLOCKCACHE -DARGO_LRU
mlockcache_la: kernelcache.c
	$(CC) $(CFLAGS) -o $@ $< -DMLOCKCACHE -DARGO_LEAST_ACCESS
clean:
	rm -fv $(OBJS)
