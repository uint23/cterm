CC = cc

CFLAGS = -std=c99 -Wall -Wextra
CPPFLAGS = -Isource/external -Isource/include

SRCS = source/cterm.c
OUT = cterm

all:
	${CC} ${SRCS} -o ${OUT}

clean:
	rm -f ${OUT}

compile_flags:
	rm -f compile_flags.txt
	for f in ${CPPFLAGS} ${CFLAGS}; do echo $$f >> compile_flags.txt; done

