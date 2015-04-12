CC = gcc
CFLAGS = -Wall -Wextra -pedantic
INCLUDES = -lportaudio -lportmidi -lsndfile -lm
INDENT = indent
INDENT_OPTS = +no-blank-lines-after-declarations +blank-lines-after-procedures +braces-on-if-line +cuddle-else +cuddle-do-while +no-space-after-function-call-names +no-space-after-cast +space-after-for +space-after-if +space-after-while +declaration-indentation1 +no-blank-lines-after-commas +braces-on-struct-decl-line +braces-on-func-def-line +dont-break-procedure-type +no-tabs +indent-level4 +preserve-mtime

.PHONY: clean indent tidy

ingrid: ingrid.o launchpad.o soundboard.o
	$(CC) -o ingrid $(INCLUDES) ingrid.o launchpad.o soundboard.o

ingrid.o: constants.h launchpad.h soundboard.h ingrid.c
	$(CC) -c ingrid.c $(CFLAGS)

launchpad.o: constants.h launchpad.h launchpad.c
	$(CC) -c launchpad.c  $(CFLAGS)

soundboard.o: constants.h launchpad.h soundboard.h soundboard.c
	$(CC) -c soundboard.c  $(CFLAGS)


indent:
	$(INDENT) $(INDENT_OPTS) -Tsound -Tlaunchpad -Tlp_event -Tsb_state ingrid.c launchpad.c soundboard.c

clean:
	-rm ingrid *.o *.c~ *.h~

tidy:
	-rm *.o *.c~ *.h~
