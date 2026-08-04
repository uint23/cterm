.POSIX:

include config.mk

CC = cc
AR = ar
ARFLAGS = -r -c
CFLAGS = -ansi -pedantic-errors -Wall -Wextra -O2
WL_CFLAGS = -std=c99 -Wall -Wextra -O2
CPPFLAGS = ${CONF_CPPFLAGS} -Iinclude -Ibuild
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
	${AR} ${ARFLAGS} $@ ${LIB_OBJS}

build/maus.o: source/maus.c include/maus.h config.mk
	mkdir -p build
	${CC} ${CFLAGS} ${CPPFLAGS} -c source/maus.c -o $@
build/maus_font.o: source/maus_font.c include/maus.h include/maus_font.h config.mk
	mkdir -p build
	${CC} ${CFLAGS} ${CPPFLAGS} -c source/maus_font.c -o $@
build/utils.o: source/utils.c include/utils.h
	mkdir -p build
	${CC} ${CFLAGS} ${CPPFLAGS} -c source/utils.c -o $@

# X11
build/maus_x11.o: source/maus_x11.c include/maus.h include/maus_x11.h config.mk
	mkdir -p build
	${CC} ${CFLAGS} ${CPPFLAGS} -c source/maus_x11.c -o $@

# Wayland
build/maus_wayland.o: source/maus_wayland.c include/maus.h include/maus_wayland.h build/xdg-shell-client-protocol.h build/pointer-constraints-unstable-v1-client-protocol.h build/relative-pointer-unstable-v1-client-protocol.h config.mk
	mkdir -p build
	${CC} ${WL_CFLAGS} ${CPPFLAGS} -c source/maus_wayland.c -o $@
build/xdg-shell-client-protocol.h:
	mkdir -p build
	wayland-scanner client-header ${PROTOS}/stable/xdg-shell/xdg-shell.xml $@
build/xdg-shell-protocol.c:
	mkdir -p build
	wayland-scanner private-code ${PROTOS}/stable/xdg-shell/xdg-shell.xml $@
build/xdg-shell-protocol.o: build/xdg-shell-protocol.c build/xdg-shell-client-protocol.h
	mkdir -p build
	${CC} ${WL_CFLAGS} ${CPPFLAGS} -c build/xdg-shell-protocol.c -o $@
build/pointer-constraints-unstable-v1-client-protocol.h:
	mkdir -p build
	wayland-scanner client-header ${PROTOS}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml $@
build/pointer-constraints-unstable-v1-protocol.c:
	mkdir -p build
	wayland-scanner private-code ${PROTOS}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml $@
build/pointer-constraints-unstable-v1-protocol.o: build/pointer-constraints-unstable-v1-protocol.c build/pointer-constraints-unstable-v1-client-protocol.h
	mkdir -p build
	${CC} ${WL_CFLAGS} ${CPPFLAGS} -c build/pointer-constraints-unstable-v1-protocol.c -o $@
build/relative-pointer-unstable-v1-client-protocol.h:
	mkdir -p build
	wayland-scanner client-header ${PROTOS}/unstable/relative-pointer/relative-pointer-unstable-v1.xml $@
build/relative-pointer-unstable-v1-protocol.c:
	mkdir -p build
	wayland-scanner private-code ${PROTOS}/unstable/relative-pointer/relative-pointer-unstable-v1.xml $@
build/relative-pointer-unstable-v1-protocol.o: build/relative-pointer-unstable-v1-protocol.c build/relative-pointer-unstable-v1-client-protocol.h
	mkdir -p build
	${CC} ${WL_CFLAGS} ${CPPFLAGS} -c build/relative-pointer-unstable-v1-protocol.c -o $@

clean:
	rm -rf build ${LIB_NAME} config.mk compile_flags.txt

compile_flags:
	rm -f compile_flags.txt
	for f in ${CFLAGS} ${CPPFLAGS}; do echo $$f >> compile_flags.txt; done

.PHONY: all clean compile_flags

