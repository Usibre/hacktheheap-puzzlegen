CC =        gcc
CFLAGS=     -O0 -gdwarf -Wl,--export-dynamic
LDFLAGS=

TARGETS=   altmalloc argv continuous-interact interact multimalloc print_env print_main tfork 
EXTRA_TARGETS= extern 


all: $(TARGETS)
	make extra

test: run.sh
	./run.sh

extra:
	$(CC) $(CFLAGS) -o compiled/libtest.so -shared lib.c
	$(CC) $(CFLAGS) -o compiled/extern -L./compiled/ extern.c -ltest
	cp extern.cfg compiled/extern.cfg 
#	sed -i "s/\[name\]/extern/g" compiled/extern.cfg

$(TARGETS): %: %.c
	$(CC) $(CFLAGS) -o compiled/$@ $<
	cp std.cfg compiled/$@.cfg 
	sed -i "s/\[name\]/$@/g" compiled/$@.cfg
clean:
	rm -rf compiled/*
