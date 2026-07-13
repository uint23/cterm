#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maus.h"
#include "maus_font.h"

void maus_draw_text(Maus* mw, MausFont* font, int32_t x, int32_t y,
                    const char* text, MausColor col)
{
	if (!mw || !mw->bfb || !font || !text)
		return;

	uint32_t col_up = MAUS_UNPACK_COL(col);
	int32_t cx = x; /* current x */
	int32_t cy = y; /* current y */

	for (size_t i = 0; text[i] != '\0'; i++) {
		char c = text[i];

		if (c == '\n') {
			cx = x;
			cy += font->cellh;
			continue;
		}

		MausGlyph* g = &font->glyphs[(uint8_t)c];
		if (!g->valid) {
			/* still advance cx if character invalid */
			cx += (font->cellw > 0 ? font->cellw : 8);
			continue;
		}

		/* origins to start drawing glyph from */
		int32_t gox = cx + g->xoff;
		int32_t goy = cy + font->asc - g->h - g->yoff;

		/* starting coordinates */
		int sgx = (-gox > 0) ? -gox : 0;
		int sgy = (-goy > 0) ? -goy : 0;

		/* ending coordinates. if glyph runs over right/bottom, clamp it */
		int end_gx = (mw->width - gox < g->w) ? (mw->width - gox) : g->w;
		int end_gy = (mw->height - goy < g->h) ?(mw->height - goy) : g->h;

		/* cull glyphs which start off screen */
		if (sgx >= end_gx || sgy >= end_gy) {
			cx += g->adv;
			continue;
		}

		/* "blit" font to framebuffer */
		for (int gy = sgy; gy < end_gy; gy++) {
			int dsty = goy + gy;

			/* calculate row pointer once per scanline */
			uint32_t* row = mw->bfb + (dsty * mw->stride);
			int offset = gy * g->w;

			for (int gx = sgx; gx < end_gx; gx++) {
				if (g->bmp[offset + gx] > 0) {
					int dstx = gox + gx;
					row[dstx] = col_up;
				}
			}
		}

		cx += g->adv;
	}
}

MausFont* maus_font_load(const char* path)
{
	FILE* fp = fopen(path, "r");
	if (!fp) {
		maus_log(stderr, "failed to load font \"%s\"", path);
		return NULL;
	}

	MausFont* font = calloc(1, sizeof(MausFont));
	if (!font) {
		maus_log(stderr, "failed to calloc font");
		fclose(fp);
		return NULL;
	}

	char line[1024];
	int asc = -1;
	int dsc = -1;
	int encoding = -1;
	int advance = 0;
	int bw = 0; /* bmp width */
	int bh = 0; /* bmp height */
	int xoff = 0;
	int yoff = 0;
	bool seen_any_glyph = 0;

	while (fgets(line, sizeof(line), fp)) {
		if (sscanf(line, "FONT_ASCENT %d", &asc) == 1)
			continue;
		if (sscanf(line, "FONT_DESCENT %d", &dsc) == 1)
			continue;

		/* ignore everything until glyphs start */
		if (strncmp(line, "STARTCHAR", 9) != 0)
			continue;

		/* reset vaules for new glyph */
		encoding = -1;
		advance = 0;
		bw = 0;
		bh = 0;
		xoff = 0;
		yoff = 0;

		while (fgets(line, sizeof(line), fp)) {
			if (sscanf(line, "ENCODING %d", &encoding) == 1)
				continue;
			if (sscanf(line, "DWIDTH %d", &advance) == 1)
				continue;
			if (sscanf(line, "BBX %d %d %d %d", &bw, &bh, &xoff, &yoff) == 4)
				continue;

			if (strncmp(line, "BITMAP", 6) == 0) {
				int rowbits;

				/* invalid character code
				   TODO: just skip it */
				if (encoding < 0 || encoding >= MAUS_BDF_GLYPHS_MAX)
					break;

				/* empty/invalid size */
				if (bw <= 0 || bh <= 0)
					break;

				MausGlyph* g = &font->glyphs[encoding];
				free(g->bmp); /* prevent leak on hot reload */
				memset(g, 0, sizeof(*g));

				g->w = bw;
				g->h = bh;
				g->xoff = xoff;
				g->yoff = yoff;
				g->adv = advance;

				g->bmp = calloc((size_t)bw * bh, 1);
				if (!g->bmp) {
					fclose(fp);
					maus_font_free(font);
					return NULL;
				}

				/* each bmp row in BDF is padded as a multiple
				   of 8 bits. this rounds width up to the next
				   multiple of 8 */
				rowbits = (bw + 7) & ~7;

				for (int y = 0; y < bh; y++) {
					uint64_t bits;

					/* missing row */
					if (!fgets(line, sizeof(line), fp)) {
						fclose(fp);
						maus_font_free(font);
						return NULL;
					}

					bits = strtoull(line, NULL, 16); /* hex to bits */
					for (int x = 0; x < bw; x++) {
						int bit = rowbits - 1 - x;

						if ((bits >> bit) & 1)
							g->bmp[y * bw + x] = 255; /* solid */
					}
				}

				g->valid = true;
				if (advance > font->cellw)
					font->cellw = advance;
				seen_any_glyph = true;

				continue;
			}

			if (strncmp(line, "ENDCHAR", 7) == 0)
				break;
		}
	}

	fclose(fp);

	if (!seen_any_glyph) {
		free(font);
		return NULL;
	}

	/* estimate asc/dsc if they werent provided */
	if (asc < 0 || dsc < 0) {
		asc = 0;
		dsc = 0;

		for (int i = 0; i < MAUS_BDF_GLYPHS_MAX; i++) {
			MausGlyph* g = &font->glyphs[i];

			/* unloaded glpyh */
			if (!g->valid)
				continue;

			/* highest point above baseline */
			if (g->h + g->yoff > asc)
				asc = g->h + g->yoff;

			/* lowest point below baseline */
			if (-g->yoff > dsc)
				dsc = -g->yoff;
		}
	}

	font->asc = asc;
	font->dsc = -dsc;
	font->cellh = asc + dsc;

	if (font->cellw <= 0 || font->cellh <= 0) {
		maus_font_free(font);
		return NULL;
	}

	return font;
}

void maus_font_free(MausFont* font)
{
	if (!font)
		return;

	for (int i = 0; i < MAUS_BDF_GLYPHS_MAX; i++)
		free(font->glyphs[i].bmp);

	free(font);
}

