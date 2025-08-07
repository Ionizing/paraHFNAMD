CC ?= gcc
CFLAGS ?= -g -O3 -DQJS_BUILD_LIBC

.PHONY: clean

all: libqjs.a test.x

test.x: test.o libqjs.a
	$(CC) -o $@ $^ $(CFLAGS) -lm -ldl -lpthread

libqjs.a: quickjs-amalgam.c.o
	ar rcs $@ $<

%.c.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)


clean:
	rm -f *.o *.a
