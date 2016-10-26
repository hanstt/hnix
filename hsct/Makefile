PREFIX?=~/opt
TARGET=sct

.PHONY: all
all: $(TARGET)

$(TARGET): sct.c
	gcc -std=c99 $(shell pkg-config --cflags x11) -o $@ $< $(shell pkg-config --libs x11 xrandr)

.PHONY: install
install: all
	mkdir -p $(PREFIX)/bin
	install $(TARGET) $(PREFIX)/bin
	install hsct.sh $(PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	rm -f $(PREFIX)/bin/hsct.sh

.PHONY: clean
clean:
	rm -f $(TARGET)
