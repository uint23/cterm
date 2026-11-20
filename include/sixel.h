#ifndef SIXEL_H
#define SIXEL_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
	uint32_t* pixels;
	int       width;
	int       height;
} Sixel;

/* decode data stream into color buffer */
Sixel sixel_decode(const char* data, size_t size);

/* free sixel image */
void sixel_free(Sixel* sixel);

#endif /* SIXEL_H */

