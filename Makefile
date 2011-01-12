OBJS = fadvcache kernelcache mlockcache usercache
CFLAGS = -Wall -Werror
all: $(OBJS)
clean:
	rm -fv $(OBJS)
