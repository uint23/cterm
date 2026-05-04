#include "font.h"
#include "schrift.h"

#include <stdlib.h>
#include <string.h>

static void blend(uint32_t* dst, int w, int h, int x, int y, uint8_t alpha,
                  uint32_t fg);

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

int font_load(Font* f, const char* path, double size)
{
	SFT_LMetrics lm;

	if (!f || !path || size <= 0)
		return -1;
	memset(f, 0, sizeof(*f));

	f->font = sft_loadfile(path);
	if (!f->font)
		return -1;

	/* configure font */
	f->size = size;
	f->sft.font = f->font;
	f->sft.xScale = size;
	f->sft.yScale = size;
	f->sft.xOffset = 0;
	f->sft.yOffset = 0;
	f->sft.flags = SFT_DOWNWARD_Y;

	/* get font line metrics */
	if (sft_lmetrics(&f->sft, &lm) < 0) {
		sft_freefont(f->font);
		memset(f, 0, sizeof(*f));
		return -1;
	}
	f->asc = lm.ascender;
	f->dsc = lm.descender;
	
	return 0;
}

void font_free(Font* f)
{
	if (!f)
		return;
	if (f->font)
		sft_freefont(f->font);
	memset(f, 0, sizeof(*f));
}

void font_draw_codepoint(Font* f, uint32_t* dst, int w, int h, int x,
                         int baseline, uint32_t cp, uint32_t fg)
{
	SFT_Glyph glyph;
	SFT_GMetrics gm;
	SFT_Image img;
	uint8_t* bmp;
	int bx;
	int by;

	/* get glyph metics */
	if (!f || !f->font || !dst)
		return;
	if (sft_lookup(&f->sft, cp, &glyph) < 0)
		return;
	if (sft_gmetrics(&f->sft, glyph, &gm) < 0)
		return;
	if (gm.minWidth <= 0 || gm.minHeight <= 0)
		return;

	bmp = calloc(gm.minWidth*gm.minHeight, 1);
	if (!bmp)
		return;

	img.pixels = bmp;
	img.width = gm.minWidth;
	img.height = gm.minHeight;

	if (sft_render(&f->sft, glyph, img) == 0) {
		/* copy bmp to dst */
		for (by = 0; by < gm.minHeight; by++) {
			for (bx = 0; bx < gm.minWidth; bx++) {
				/* TODO: proper aa controls */
				uint8_t a = bmp[(by*gm.minWidth) + bx]; /* alpha */
				uint8_t aa = f->aa ? a : (a > 128 ? 255 : 0);

				/* translate bpm pixels to dst pixels */
				int dst_x = x + (int)gm.leftSideBearing + bx;
				int dst_y = baseline + gm.yOffset + by;

				blend(dst, w, h, dst_x, dst_y, aa, fg);
			}
		}
	}
	free(bmp);
}

