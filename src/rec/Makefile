CC =        gcc
CFLAGS =    -fPIC -Wall -Wextra -O3 -Wno-unused-parameter
LDFLAGS =   -fPIC -z muldefs
OUT =       hth_console
EXTRA_CFLAGS =

all: main.o console.o manager.o export.o
	$(CC) export.o console.o manager.o main.o -o $(OUT) $(LDFLAGS)

install: $(OUT)
	sudo cp $(OUT) /usr/bin/

main.o: main.c
	$(CC) main.c -c -o main.o $(CFLAGS) $(EXTRA_CFLAGS)

console.o: console.c console.h main.h
	$(CC) console.c -c -o console.o $(CFLAGS) $(EXTRA_CFLAGS)

manager.o: manager.c manager.h main.h
	$(CC) manager.c -c -o manager.o $(CFLAGS) $(EXTRA_CFLAGS)

export.o: export.c export.h manager.h main.h
	$(CC) export.c -c -o export.o $(CFLAGS) $(EXTRA_CFLAGS)

clean:
	rm -f *.o core
