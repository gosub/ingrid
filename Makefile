CC = gcc
CFLAGS = -Wall -Wextra -pedantic
INCLUDES = -lportaudio -lportmidi -lsndfile -lm

.PHONY: clean

ingrid: ingrid.o launchpad.o soundboard.o
	$(CC) -o ingrid $(INCLUDES) ingrid.o launchpad.o soundboard.o

ingrid.o: constants.h launchpad.h soundboard.h ingrid.c
	$(CC) -c ingrid.c $(CFLAGS)

launchpad.o: constants.h launchpad.h launchpad.c
	$(CC) -c launchpad.c  $(CFLAGS)

soundboard.o: constants.h launchpad.h soundboard.h soundboard.c
	$(CC) -c soundboard.c  $(CFLAGS)

clean:
	rm ingrid *.o 
