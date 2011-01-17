OBJS = kernelcache fadvcache_lru mlockcache_lru fadvcache_la mlockcache_la client
CFLAGS = -Wall -Werror -g
all: $(OBJS)
client: client.c
	$(CC) $(CFLAGS) -o $@ $<
kernelcache: kernelcache.c
	$(CC) $(CFLAGS) -o $@ $< -DARGO_LEAST_ACCESS
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
