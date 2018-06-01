CCOPTS= -Wall -g -std=gnu99 -Wstrict-prototypes
LIBS=
CC=gcc
AR=ar


BINS = main

OBJS = main.c simplefs.c disk_driver.c bitmap.c

HEADERS = simplefs.h\
		  disk_driver.h\
		  bitmap.h

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

.phony: clean all


all:	$(BINS)

main: main.c $(OBJS)
	$(CC) $(CCOPTS)  -o $@ $^ $(LIBS)

clean:
	rm -rf *.o *~  main

test: ./main
