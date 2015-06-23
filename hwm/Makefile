PREFIX=~/opt

CC=ccache gcc

CPPFLAGS:=-I/usr/local/include -I$(HUTILS_PATH)/include `pkg-config --cflags xcb`
CFLAGS:=-ansi -ggdb -pedantic-errors -Wall -Werror -Wextra -Wmissing-prototypes -Wshadow #-Wuninitialized
LDFLAGS:=`pkg-config --libs xcb-icccm xcb-keysyms xcb-randr` -L/usr/local/lib -liconv

all: hwm

hwm: hwm.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f hwm

install: all
	mkdir -p $(PREFIX)/bin
	install hwm $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/hwm

.PHONY: all clean dist install uninstall
