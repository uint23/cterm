#include <stdlib.h>
#include <string.h>

#include "utils.h"

char* strdup(const char* src)
{
	if (src == NULL)
		return NULL;

	size_t len = strlen(src);
	char* dst = malloc(len + 1);
	if (dst == NULL)
		return NULL;

	memcpy(dst, src, len + 1);
	return dst;
}

