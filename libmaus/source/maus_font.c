#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maus.h"
#include "maus_font.h"

static void glyph_to_buf(int32_t sgx, int32_t egx, int32_t sgy, int32_t egy,
                         int32_t gox, int32_t goy, MausGlyph* g, uint32_t col_up,
                         uint32_t* buf, uint32_t stride);

/* "blit" a glpyh to some buffer */
static void glyph_to_buf(int32_t sgx, int32_t egx, int32_t sgy, int32_t egy,
                         int32_t gox, int32_t goy, MausGlyph* g, uint32_t col_up,
                         uint32_t* buf, uint32_t stride)
{
	int gy;
	int gx;
	int32_t dstx;

	uint32_t* row;
	int32_t offset;

	for (gy = sgy; gy < egy; gy++) {
		int dsty = goy + gy;

		/* calculate row pointer once per scanline */
		row = buf + (dsty * stride);
		offset = gy * g->w;

		for (gx = sgx; gx < egx; gx++) {
			if (g->bmp[offset + gx] > 0) {
				dstx = gox + gx;
				row[dstx] = col_up;
			}
		}
	}
}

static int8_t parse_glyphs(FILE* f, MausFont* font, char* line, size_t line_size,
                           int32_t* encoding, int32_t* advance, int32_t* bw, int32_t* bh,
                           int32_t* xoff, int32_t* yoff, int8_t* seen)
{
	int32_t rowbits;

	int32_t x;
	int32_t y;
	uint64_t bits;

	MausGlyph* g;

	while (fgets(line, line_size, f)) {
		if (sscanf(line, "ENCODING %d", encoding) == 1)
			continue;
		if (sscanf(line, "DWIDTH %d", advance) == 1)
			continue;
		if (sscanf(line, "BBX %d %d %d %d", bw, bh, xoff, yoff) == 4)
			continue;

		if (strncmp(line, "BITMAP", 6) == 0) {
			/* invalid character code
			   TODO: just skip it */
			if (*encoding < 0 || *encoding >= MAUS_BDF_GLYPHS_MAX)
				break;

			/* empty/invalid size */
			if (*bw <= 0 || *bh <= 0)
				break;

			g = &font->glyphs[*encoding];
			free(g->bmp); /* prevent leak on hot reload */
			memset(g, 0, sizeof(*g));

			g->w = *bw;
			g->h = *bh;
			g->xoff = *xoff;
			g->yoff = *yoff;
			g->adv = *advance;

			g->bmp = calloc((size_t)(*bw) * (*bh), 1);
			if (!g->bmp) {
				fclose(f);
				maus_font_free(font);
				return 0;
			}

			/* each bmp row in BDF is padded as a multiple
			   of 8 bits. this rounds width up to the next
			   multiple of 8 */
			rowbits = (*bw + 7) & ~7;

			for (y = 0; y < *bh; y++) {
				/* missing row */
				if (!fgets(line, line_size, f)) {
					fclose(f);
					maus_font_free(font);
					return 0;
				}

				bits = strtoull(line, NULL, 16); /* hex to bits */
				for (x = 0; x < *bw; x++) {
					int bit = rowbits - 1 - x;

					if ((bits >> bit) & 1)
						g->bmp[y * (*bw) + x] = 255; /* solid */
				}
			}

			g->valid = 1;
			if (*advance > font->cellw)
				font->cellw = *advance;
			*seen = 1;

			continue;
		}

		if (strncmp(line, "ENDCHAR", 7) == 0)
			break;
	}

	return 1;
}

void maus_draw_text(Maus* mw, MausFont* font, int32_t x, int32_t y,
                    const char* text, MausColor col)
{
	uint32_t col_up = MAUS_UNPACK_COL(col);

	size_t i;

	/* current coordinates */
	int32_t cx = x;
	int32_t cy = y;

	/* origins to start drawing glyph from */
	int32_t gox;
	int32_t goy;

	/* starting glyph coordinates */
	int32_t sgx;
	int32_t sgy;

	/* ending glyph coordinates */
	int32_t egx;
	int32_t egy;

	/* current glyph pointer */
	MausGlyph* g;

	if (!mw || !mw->bfb || !font || !text)
		return;

	for (i = 0; text[i] != '\0'; i++) {
		char c = text[i];

		if (c == '\n') {
			cx = x;
			cy += font->cellh;
			continue;
		}

		g = &font->glyphs[(uint8_t)c];
		if (!g->valid) {
			/* still advance cx if character invalid */
			cx += (font->cellw > 0 ? font->cellw : 8);
			continue;
		}

		/* origins to start drawing glyph from */
		gox = cx + g->xoff;
		goy = cy + font->asc - g->h - g->yoff;

		/* starting coordinates */
		sgx = (-gox > 0) ? -gox : 0;
		sgy = (-goy > 0) ? -goy : 0;

		egx = (mw->width - gox < g->w) ? (mw->width - gox) : g->w;
		egy = (mw->height - goy < g->h) ?(mw->height - goy) : g->h;

		/* cull glyphs which start off screen */
		if (sgx >= egx || sgy >= egy) {
			cx += g->adv;
			continue;
		}

		glyph_to_buf(sgx, egx, sgy, egy, gox, goy, g, col_up, mw->bfb, mw->stride);
		cx += g->adv;
	}
}

MausFont* maus_font_load(const char* path)
{
	FILE* f = fopen(path, "r");
	MausFont* font = calloc(1, sizeof(MausFont));
	char line[1024];

	int32_t asc = -1;
	int32_t dsc = -1;
	int32_t encoding = -1;
	int32_t advance = 0;
	int32_t bw = 0; /* bmp width */
	int32_t bh = 0; /* bmp height */
	int32_t xoff = 0;
	int32_t yoff = 0;
	int8_t seen = 0;

	int i;

	if (!f) {
		maus_log(stderr, "failed to load font \"%s\"", path);
		return NULL;
	}

	if (!font) {
		maus_log(stderr, "failed to calloc font");
		fclose(f);
		return NULL;
	}

	while (fgets(line, sizeof(line), f)) {
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

		if (!parse_glyphs(f, font, line, sizeof(line), &encoding, &advance, &bw, &bh, &xoff, &yoff, &seen))
			return NULL;
	}


	fclose(f);

	if (!seen) {
		free(font);
		return NULL;
	}

	/* estimate asc/dsc if they werent provided */
	if (asc < 0 || dsc < 0) {
		asc = 0;
		dsc = 0;

		for (i = 0; i < MAUS_BDF_GLYPHS_MAX; i++) {
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
	int i;

	if (!font)
		return;

	for (i = 0; i < MAUS_BDF_GLYPHS_MAX; i++)
		free(font->glyphs[i].bmp);

	free(font);
}

