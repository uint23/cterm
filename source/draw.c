#include <stdlib.h>
#include <string.h>

#include "draw.h"

static void blend(uint32_t* dst, int w, int h, int x, int y, uint8_t alpha,
                  uint32_t fg);
static Glyph* get_glyph(Fontface* f, uint32_t cp);

static Glyph glyphs[GLYPH_CACHE_MAX];

/**
 * @brief blend a pixel using alpha compositong
 *
 * @param dst buffer to write to
 * @param x pixel x-position
 * @param y piyel y-position
 * @param alpha coverage from glyph
 * @param fg foreground colour
 */
static void blend(uint32_t* dst, int w, int h, int x, int y, uint8_t alpha,
                  uint32_t fg)
{
	uint8_t* px;
	uint8_t fg_r;
	uint8_t fg_g;
	uint8_t fg_b;

	if (x < 0 || x >= w || y < 0 || y >= h || alpha == 0)
		/* transparent/oob */
		return;

	/* ptr to px at (x, y) */
	px = (uint8_t*)&dst[y * w + x];

	/* unpack fg colour*/
	fg_r = (uint8_t)(fg & 0xff);
	fg_g = (uint8_t)((fg >> 8) & 0xff);
	fg_b = (uint8_t)((fg >> 16) & 0xff);

	/* alpha blend each channel */
	px[0] = (uint8_t)((fg_r * alpha + px[0] * (255 - alpha)) / 255);
	px[1] = (uint8_t)((fg_g * alpha + px[1] * (255 - alpha)) / 255);
	px[2] = (uint8_t)((fg_b * alpha + px[2] * (255 - alpha)) / 255);

	px[3] = 255;
}

/** TODO
 */
static Glyph* get_glyph(Fontface* f, uint32_t cp)
{
	SFT_Glyph glyph;
	SFT_GMetrics gm;
	SFT_Image img;
	Glyph* g;

	g = &glyphs[cp % GLYPH_CACHE_MAX];
	if (g->valid && g->cp == cp)
		return g;
	free(g->bmp);
	memset(g, 0, sizeof(*g));

	if (sft_lookup(&f->sft, cp, &glyph) < 0)
		return NULL;
	if (sft_gmetrics(&f->sft, glyph, &gm) < 0)
		return NULL;
	if (gm.minWidth <= 0 || gm.minHeight <= 0)
		return NULL;

	g->bmp = calloc(gm.minWidth * gm.minHeight, 1);
	if (!g->bmp)
		return NULL;

	img.pixels = g->bmp;
	img.width = gm.minWidth;
	img.height = gm.minHeight;

	if (sft_render(&f->sft, glyph, img) < 0) {
		free(g->bmp);
		memset(g, 0, sizeof(*g));
		return NULL;
	}

	g->w = gm.minWidth;
	g->h = gm.minHeight;
	g->lsb = gm.leftSideBearing;
	g->yoff = gm.yOffset;
	g->cp = cp;
	g->valid = true;

	return g;
}

void draw_clear(uint32_t* dst, int w, int h, uint32_t col)
{
	for (int i = 0; i < w*h; i++)
		dst[i] = col;
}

void draw_rune(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
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

void draw_caret(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
                 uint32_t fg)
{
	draw_rune(dst, w, h, cl, rw, cw, ch, fg);
}

void draw_codepoint(Fontface* f, uint32_t* dst, int w, int h, int x,
                    int baseline, uint32_t cp, uint32_t fg)
{
	Glyph* g;

	if (!f || !f->font || !dst)
		return;

	g = get_glyph(f, cp);
	if (!g)
		return;

	for (int by = 0; by < g->h; by++) {
		for (int bx = 0; bx < g->w; bx++) {
			uint8_t a = g->bmp[(by * g->w) + bx];
			uint8_t aa = f->aa ? a : (a > 128 ? 255 : 0);

			int dst_x = x + g->lsb + bx;
			int dst_y = baseline + g->yoff + by;

			blend(dst, w, h, dst_x, dst_y, aa, fg);
		}
	}
}

