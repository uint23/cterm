include config.mk

CC = cc
AR = ar
CFLAGS = -std=c99 -Wall -Wextra -O2
CPPFLAGS = ${CONF_CPPFLAGS} -Iinclude
LDFLAGS = ${CONF_LDFLAGS}

LIB_NAME = build/${CONF_LIB_NAME}
LIB_SRCS = source/maus.c      \
           source/maus_font.c \
           source/utils.c     \
           ${CONF_SRC}
LIB_OBJS = build/maus.o       \
           build/maus_font.o  \
           build/utils.o      \
           ${CONF_OBJS}

all: ${LIB_NAME}

${LIB_NAME}: ${LIB_OBJS}
	${AR} -rcs $@ ${LIB_OBJS}

build/maus.o: source/maus.c
	mkdir -p build
	${CC} ${CFLAGS} ${CPPFLAGS} -c source/maus.c -o $@
build/maus_font.o: source/maus_font.c
	mkdir -p build
	${CC} ${CFLAGS} ${CPPFLAGS} -c source/maus_font.c -o $@
build/utils.o: source/utils.c
	mkdir -p build
	${CC} ${CFLAGS} ${CPPFLAGS} -c source/utils.c -o $@
build/maus_x11.o: source/maus_x11.c
	mkdir -p build
	${CC} ${CFLAGS} ${CPPFLAGS} -c source/maus_x11.c -o $@

clean:
	rm -rf build ${LIB_NAME} config.mk compile_flags.txt

compile_flags:
	rm -f compile_flags.txt
	for f in ${CFLAGS} ${CPPFLAGS}; do echo $$f >> compile_flags.txt; done

.PHONY: all clean compile_flags

