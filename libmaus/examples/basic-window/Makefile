include ../../config.mk

CC = cc
CFLAGS = -std=c99 -Wall -Wextra -g
CPPFLAGS = ${CONF_CPPFLAGS} -I../../include -I../../build
LIBS = ${CONF_LDFLAGS}
MAUS_LIB = ../../build/${CONF_LIB_NAME}

SRC = main.c

all:
	${CC} ${CFLAGS} ${CPPFLAGS} ${SRC} ${MAUS_LIB} ${LIBS} -o basic-window
