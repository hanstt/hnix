PREFIX?=~/opt
TARGET=hbat

.PHONY: all
all: $(TARGET)

$(TARGET): main.c
	gcc -ansi -O2 -pedantic-errors -Wall -Werror -Wextra -Wmissing-prototypes -Wshadow -Wstrict-prototypes -o $@ main.c

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
