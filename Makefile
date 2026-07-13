CC = cc
CFLAGS = -std=c99 -Wall -Wextra
CPPFLAGS = -Isource/include -Ilibmaus/include

LDFLAGS = -lschrift -lgrapheme -lm

include platform.mk

SRCS = source/cterm.c \
       source/utils.c  \
       source/font.c  \
       source/term.c  \
       source/draw.c  \
       libmaus/source/maus.c     \
       libmaus/source/maus_x11.c \
       libmaus/source/utils.c
OUT = cterm

all:
	${CC} ${SRCS} ${CFLAGS} -O2 ${CPPFLAGS} ${LDFLAGS} -o ${OUT}

debug:
	${CC} ${SRCS} ${CFLAGS} -g ${CPPFLAGS} ${LDFLAGS} -o ${OUT}

clean:
	rm -f ${OUT}

compile_flags:
	rm -f compile_flags.txt
	for f in ${CPPFLAGS} ${CFLAGS}; do echo $$f >> compile_flags.txt; done

