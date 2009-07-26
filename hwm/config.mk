VERSION=custom

PREFIX=/opt

LIBS=-lX11

# Xinerama for multiple displays.
#LIBS+=-lXinerama
#OPT+=-DXINERAMA

# XKB for switching keyboard layout.
LIBS+=-lxkbfile
OPT+=-DXKB

CPPFLAGS=-DVERSION=\"${VERSION}\" ${OPT}
CFLAGS=-ggdb -std=c99 -pedantic-errors -Wall -Wmissing-prototypes -Wswitch-enum ${CPPFLAGS}
LDFLAGS=${LIBS}

#CFLAGS=-fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS=${LIBS}

CC=cc
