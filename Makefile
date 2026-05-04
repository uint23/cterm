CC = cc
CFLAGS = -std=c99 -Wall -Wextra
CPPFLAGS = -Isource/external -Isource/include
LDFLAGS = -framework Cocoa -framework CoreVideo -framework IOKit -framework CoreGraphics -framework CoreFoundation -framework Carbon

SRCS = source/cterm.c \
       source/util.c  \
       source/draw.c
OUT = cterm

all:
	${CC} ${SRCS} ${CFLAGS} ${CPPFLAGS} ${LDFLAGS} -o ${OUT}

clean:
	rm -f ${OUT}

compile_flags:
	rm -f compile_flags.txt
	for f in ${CPPFLAGS} ${CFLAGS}; do echo $$f >> compile_flags.txt; done

