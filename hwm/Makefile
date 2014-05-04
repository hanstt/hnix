PREFIX=~/opt

CC=cc

CFLAGS:=-ansi -O2 -pedantic-errors -Wall -Werror -Wextra -Wmissing-prototypes -Wshadow -Wuninitialized
LDFLAGS:=`pkg-config xcb-icccm --libs`

all: hwm

hwm: hwm.c Makefile
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f hwm

install: all
	mkdir -p $(PREFIX)/bin
	cp -f hwm $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/hwm

uninstall:
	rm -f $(PREFIX)/bin/hwm

.PHONY: all clean dist install uninstall
