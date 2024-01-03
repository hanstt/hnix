PREFIX=$(HOME)/opt

CPPFLAGS:=-I/usr/local/include $(shell pkg-config --cflags xcb)
CFLAGS:=-ansi -ggdb -pedantic-errors -Wall -Werror -Wformat=2 -Wmissing-prototypes -Wshadow -Wstrict-prototypes
LDFLAGS:=-L/usr/local/lib
LIBS:=$(shell pkg-config --libs xcb-icccm xcb-keysyms xcb-randr)

.PHONY: all
all: hwm

hwm: hwm.c config.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

config.h: config.def.h
	if test -f $@; then mv $@ $@.bak; fi
	cp $< $@

.PHONY: clean
clean:
	rm -f hwm

.PHONY: install
install: hwm
	mkdir -p $(PREFIX)/bin
	install hwm $(PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm -f $(PREFIX)/bin/hwm
