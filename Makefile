ifeq ($(COMPILER),)
	COMPILER := gcc
endif

COMPILE		= $(COMPILER) $(strip -std=c11 -Wall -Wextra -g -O0 $(CFLAGS))
PROGNAME	= ttylog

.PHONY: test clean

all: $(PROGNAME)

$(PROGNAME): $(PROGNAME).o
	$(COMPILE) $< -o $@

$(PROGNAME).o: $(PROGNAME).c $(PROGNAME.h)
	$(COMPILE) -c $< -o $@

test: $(PROGNAME)
	./$(PROGNAME) irssi

clean:
	rm -f $(PROGNAME) *.o
