#include "draw.h"

void draw_clear(uint32_t* dst, int w, int h, uint32_t col)
{
	for (int i = 0; i < w*h; i++)
		dst[i] = col;
}

void draw_cell(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
               uint32_t bg)
{
	int x0 = cl * cw; /* x origin of cell */
	int y0 = rw * ch; /* y origin of cell */
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

void draw_cursor(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
                 uint32_t fg)
{
	draw_cell(dst, w, h, cl, rw, cw, ch, fg);
}

