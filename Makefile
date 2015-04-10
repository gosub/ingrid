CC = gcc
CFLAGS = -Wall -Wextra -pedantic
INCLUDES = -lportaudio -lportmidi -lsndfile -lm

.PHONY: clean

ingrid: ingrid.o launchpad.o
	$(CC) -o ingrid $(INCLUDES) ingrid.o launchpad.o

ingrid.o: ingrid.c launchpad.h
	gcc -c ingrid.c $(CFLAGS)

launchpad.o: launchpad.c launchpad.h
	gcc -c launchpad.c  $(CFLAGS)

clean:
	rm ingrid *.o 
