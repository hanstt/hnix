PREFIX=~/opt

LIBS=-lX11

CPPFLAGS+=-DXINERAMA
LIBS+=-lXinerama

CPPFLAGS+=-DXKB
LIBS+=-lxkbfile

CFLAGS=-ggdb -std=c99 -pedantic-errors -Wall -Wmissing-prototypes -Wswitch-enum ${CPPFLAGS}
LDFLAGS=${LIBS}

CC=cc
