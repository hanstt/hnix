include config.mk

all: hwm.c config.h
	${CC} ${CFLAGS} -o hwm hwm.c ${LDFLAGS}

clean:
	rm -f hwm hwm.o

install: all
	mkdir -p ${PREFIX}/bin
	cp -f hwm ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/hwm

uninstall:
	rm -f ${PREFIX}/bin/hwm

.PHONY: all clean dist install uninstall
