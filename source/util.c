#include <stdio.h>
#include <stdlib.h>

#include "util.h"

void die(int ec, const char* msg)
{
	perror(msg);
	exit(ec);
}

uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	return
		((uint32_t)a << 24) |
		((uint32_t)b << 16) |
		((uint32_t)g << 8)  |
		((uint32_t)r);
}

void* xmalloc(size_t len)
{
	void* p = malloc(len);
	if (!p)
		die(EXIT_FAILURE, "malloc failed");
	return p;
}

