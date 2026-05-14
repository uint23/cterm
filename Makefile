CC = cc
CFLAGS = -std=c99 -Wall -Wextra
CPPFLAGS = -Isource/external -Isource/include \
	   -D_POSIX_C_SOURCE=200112L

LDFLAGS = -lschrift -lgrapheme -lm

include platform.mk

SRCS = source/cterm.c \
       source/utils.c  \
       source/font.c  \
       source/term.c  \
       source/draw.c
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

