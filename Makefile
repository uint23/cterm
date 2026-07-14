CC = cc
CFLAGS = -std=c99 -Wall -Wextra
CPPFLAGS = -Iinclude -Ilibmaus/include
LDFLAGS =
LDLIBS = -lgrapheme

include platform.mk

SRCS = source/cterm.c \
       source/utils.c \
       source/font.c \
       source/term.c \
       source/draw.c

OUT = cterm

all:
	${CC} ${CFLAGS} -Os ${CPPFLAGS} ${SRCS} ${LDFLAGS} -o ${OUT} ${LDLIBS}

debug:
	${CC} ${CFLAGS} -g  ${CPPFLAGS} ${SRCS} ${LDFLAGS} -o ${OUT} ${LDLIBS}

clean:
	rm -f ${OUT}

compile_flags:
	rm -f compile_flags.txt
	for f in ${CPPFLAGS} ${CFLAGS}; do echo $$f >> compile_flags.txt; done

.PHONY: all debug clean compile_flags

