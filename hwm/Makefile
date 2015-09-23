PREFIX=~/opt

CC=ccache gcc

CPPFLAGS:=-I/usr/local/include -I$(HUTILS_PATH)/include `pkg-config --cflags xcb`
CFLAGS:=-ansi -ggdb -pedantic-errors -Wall -Werror -Wextra -Wmissing-prototypes -Wshadow
LDFLAGS:=`pkg-config --libs xcb-icccm xcb-keysyms xcb-randr` -L/usr/local/lib -liconv

.PHONY: all
all: hwm

hwm: hwm.c config.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

config.h: config.def.h
	mv $@ $@.bak
	cp $< $@

.PHONY: clean
clean:
	rm -f hwm

.PHONY: install
install: all
	mkdir -p $(PREFIX)/bin
	install hwm $(PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm -f $(PREFIX)/bin/hwm
