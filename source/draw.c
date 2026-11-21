#include "draw.h"

void draw_rune(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
               uint32_t bg)
{
	int x0 = cl; /* x origin of cell */
	int y0 = rw; /* y origin of cell */
	int x;
	int y;

	/* iterate through columns */
	for (y = y0; y < y0+ch && y < h; y++) {
		if (y < 0)
			continue;


		/* through rows */
		for (x = x0; x < x0+cw && x < w; x++) {
			if (x < 0)
				continue;
			dst[y * w + x] = bg;
		}
	}
}

