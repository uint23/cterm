#include <stdlib.h>
#include <string.h>

#include "utils.h"

char* maus_strdup(const char* src)
{
	size_t len;
	char* dst;
	if (src == NULL)
		return NULL;

	len = strlen(src);
	dst = malloc(len + 1);
	if (dst == NULL)
		return NULL;

	memcpy(dst, src, len + 1);
	return dst;
}

