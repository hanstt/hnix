PREFIX=~/opt

CC=ccache gcc

CFLAGS:=-ansi -ggdb -O2 -pedantic-errors -Wall -Werror -Wextra -Wmissing-prototypes -Wshadow -Wuninitialized
LDFLAGS:=`pkg-config --libs xcb-icccm xcb-randr`

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
