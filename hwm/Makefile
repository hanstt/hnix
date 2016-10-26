PREFIX=~/opt

CC?=ccache gcc

include $(HTOOLS_PATH)/hgmake/build_dir.mk

CPPFLAGS:=-I/usr/local/include -I$(HTOOLS_PATH)/hutils/include -I$(HTOOLS_PATH)/hutils/$(BUILD_DIR) `pkg-config --cflags xcb`
include $(HTOOLS_PATH)/hgmake/strict_cflags.mk
CFLAGS:=$(CFLAGS) -ggdb
LDFLAGS:=-L/usr/local/lib
LIBS:=`pkg-config --libs xcb-icccm xcb-keysyms xcb-randr` -liconv

.PHONY: all
all: hwm

hwm: hwm.c config.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

config.h: config.def.h
	test -f $@ && mv $@ $@.bak
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
