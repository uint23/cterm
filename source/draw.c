#include "draw.h"
#include "sixel.h"

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

void draw_sixel(uint32_t* dst, int stride, int height, const Sixel* img, int x,
		int y, int left, int top, int right, int bottom)
{
	for (int row = 0; row < img->height; row++) {
		int py = y + row;
		if (py < 0 || py >= height || py < top || py >= bottom)
			continue;

		for (int col = 0; col < img->width; col++) {
			int px = x + col;
			if (px < 0 || px >= stride || px < left || px >= right)
				continue;

			uint32_t color = img->pixels[row * img->width + col];
			if (color >> 24)
				dst[py * stride + px] = color;
		}
	}
}

