include config.mk

all: dwm.o
	${CC} -o dwm $^ ${LDFLAGS}

dwm.o: dwm.c config.h
	${CC} -c ${CFLAGS} $<

clean:
	rm -f dwm dwm.o

install: all
	mkdir -p ${PREFIX}/bin
	cp -f dwm ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/dwm

uninstall:
	rm -f ${PREFIX}/bin/dwm

.PHONY: all clean dist install uninstall
