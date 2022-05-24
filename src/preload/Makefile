CC =            gcc
CFLAGS =        -fPIC -Wall -Wextra -O3 -D_GNU_SOURCE -Wno-unused-parameter
LDFLAGS =       -shared -fPIC -ldl
OUT =           libhtheap.so
MALLOC =  malloc
FREE =    free
CALLOC =  calloc
REALLOC = realloc
NOCORRUPTION = 0
EXTRA_CFLAGS =
INSTALL_LOCATION = /usr/lib/
DEBUG = 0

all: main.o
	$(CC) main.o -o $(OUT) $(LDFLAGS)

install: $(OUT)
	sudo cp $(OUT) $(INSTALL_LOCATION)

main.o: main.c
	$(CC) main.c -c -o main.o $(CFLAGS) $(EXTRA_CFLAGS) -DMALMACRO=$(MALLOC) -DFREEMACRO=$(FREE) -DCALLOCMACRO=$(CALLOC) -DREALLOCMACRO=$(REALLOC) -DDBG=$(DEBUG) -DNOCORRUPTION=$(NOCORRUPTION)

clean:
	rm -f *.o core

help:
	echo 'e.g. "make MALLOC=ngx_malloc NOCORRUPTION=1 DEBUG=1"'
