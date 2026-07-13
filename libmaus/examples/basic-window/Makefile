CC = cc
CFLAGS = -std=c99 -Wall -Wextra -g
CPPFLAGS = -DBACKEND_X11 -I../../include
LIBS = -lX11 -lXext
MAUS_LIB = ../../build/libmaus_x11.a

SRC = main.c

all:
	${CC} ${CFLAGS} ${CPPFLAGS} ${SRC} ${MAUS_LIB} ${LIBS} -o basic-window
