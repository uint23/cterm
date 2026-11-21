#include "draw.h"
#include "sixel.h"

static void draw_bitmap(uint32_t* dst, int w, int h, uint8_t* bmp,
                        int bw, int bh, int x, int y, uint32_t fg);

/* draw bitmap of properties (look: vvvv) to screen */
static void draw_bitmap(uint32_t* dst, int w, int h, uint8_t* bmp,
		int bw, int bh, int x, int y, uint32_t fg)
{
	for (int by = 0; by < bh; by++) {
		int py = y + by;
		if (py < 0 || py >= h)
			continue;

		for (int bx = 0; bx < bw; bx++) {
			int px = x + bx;
			if (px < 0 || px >= w)
				continue;

			if (bmp[by * bw + bx])
				dst[py * w + px] = fg;
		}
	}
}

void draw_codepoint(Fontface* f, uint32_t* dst, int w, int h, int x,
		int baseline, uint32_t cp, uint32_t fg)
{
	if (!f || !dst)
		return;

	if (!f->bdf || cp >= BDF_GLYPHS_MAX)
		return;

	BdfGlyph* g = &f->bdf[cp];
	if (!g->valid)
		return;

	draw_bitmap(
		dst, w, h, g->bmp, g->w, g->h, x + g->xoff,
		baseline - g->yoff - g->h, fg
	);
}

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

