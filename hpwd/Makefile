PREFIX?=~/opt
TARGET=hpwd

include $(HTOOLS_PATH)/hgmake/strict_cflags.mk

.PHONY: all
all: $(TARGET)

$(TARGET): main.c
	gcc $(CFLAGS) -O2 -o $@ $<

.PHONY: install
install:
	mkdir -p $(PREFIX)/bin
	install $(TARGET) $(PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)
