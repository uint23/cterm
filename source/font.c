#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "schrift.h"

static int load_bdf(Fontface* f, const char* path);
static int load_sft(Fontface* f, const char* path, double size);
static int strcicmp(char const* a, char const* b);
static int type(const char* path);

/*
 * @brief load a bdf font
 *
 * @param f fontface to load font into
 * @param path path to load font from
 *
 * @return -1=fail, 0=success
 */
static int load_bdf(Fontface* f, const char* path)
{
	f->kind = FONT_BDF;

	FILE* fp;
	fp = fopen(path, "r");
	if (!fp)
		return -1;

	char line[1024];
	int asc = -1;
	int dsc = -1;
	int encoding = -1;
	int advance = 0;
	int bw = 0; /* bmp width */
	int bh = 0; /* bmp height */
	int xoff = 0;
	int yoff = 0;
	int seen_any_glyph = 0;

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
			if (sscanf(line, "BBX %d %d %d %d",
			    &bw, &bh, &xoff, &yoff) == 4)
				continue;

			if (strncmp(line, "BITMAP", 6) == 0) {
				BdfGlyph* g;
				int rowbits;

				/* invalid character code 
				   TODO: just skip it
				 */
				if (encoding < 0 || encoding >= BDF_GLYPHS_MAX)
					break;

				/* empty/invalid size */
				if (bw <= 0 || bh <= 0)
					break;

				g = &f->bdf[encoding];
				free(g->bmp); /* prevent leak on reset */
				memset(g, 0, sizeof(*g));

				g->w = bw;
				g->h = bh;
				g->xoff = xoff;
				g->yoff = yoff;
				g->adv = advance;

				g->bmp = calloc((size_t)bw * bh, 1);
				if (!g->bmp) {
					fclose(fp);
					font_free(f);
					return -1;
				}

				/* each bmp row in BDF is padded as a multiple
				   of 8 bits. this rounds width up to the next
				   multiple of 8
				 */
				rowbits = (bw + 7) & ~7;

				for (int y = 0; y < bh; y++) {
					uint64_t bits;

					/* missing row */
					if (!fgets(line, sizeof(line), fp)) {
						fclose(fp);
						font_free(f);
						return -1;
					}

					bits = strtoull(line, NULL, 16); /* hex to bits */
					for (int x = 0; x < bw; x++) {
						int bit = rowbits - 1 - x;

						if ((bits >> bit) & 1)
							g->bmp[y * bw + x] = 255; /* solid */
					}
				}

				g->valid = 1;
				if (advance > f->cellw)
					f->cellw = advance;
				seen_any_glyph = 1;

				continue;
			}

			if (strncmp(line, "ENDCHAR", 7) == 0)
				break;
		}
	}

	fclose(fp);

	if (!seen_any_glyph)
		return -1;

	/* estimate asc/dsc if they werent provided */
	if (asc < 0 || dsc < 0) {
		asc = 0;
		dsc = 0;

		for (int i = 0; i < BDF_GLYPHS_MAX; i++) {
			BdfGlyph* g = &f->bdf[i];

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

	f->asc = asc;
	f->dsc = -dsc;
	f->cellh = asc + dsc;
	f->size = f->cellh;

	if (f->cellw <= 0 || f->cellh <= 0) {
		font_free(f);
		return -1;
	}

	return 0;
}

/*
 * @brief load an sft font
 *
 * @param f fontface to load font into
 * @param path path to load font from
 * @param size size to load font as
 *
 * @return -1=failed to load font, 0=success
 */
static int load_sft(Fontface* f, const char* path, double size)
{
	f->kind = FONT_SFT;

	SFT_LMetrics lm;
	SFT_Glyph glyph;
	SFT_GMetrics gm;

	if (!f || !path || size <= 0)
		return -1;

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
	if (sft_lmetrics(&f->sft, &lm) < 0)
		goto fail;

	f->asc = lm.ascender;
	f->dsc = lm.descender;

	/* cell height */
	f->cellh = (int)ceil(lm.ascender - lm.descender + lm.lineGap);

	/* cell width */
	if (sft_lookup(&f->sft, 'A', &glyph) < 0)
		goto fail;
	if (sft_gmetrics(&f->sft, glyph, &gm) < 0)
		goto fail;

	f->cellw = (int)ceil(gm.advanceWidth);

	if (f->cellw <= 0 || f->cellh <= 0)
		goto fail;
	
	return 0;

fail:
	font_free(f);
	return -1;
}

/**
 * @brief case insensitive string compare
 *
 * @param a string1 to compare
 * @param b string2 to compare
 *
 * @return -1=(a<b), 0=(a==b), 1=(a>b)
 */
static int strcicmp(char const* a, char const* b)
{
	int la;
	int lb;
	while (*a && *b) {
		la = tolower((unsigned char)*a);
		lb = tolower((unsigned char)*b);
		if (la != lb)
			return la - lb;
		a++;
		b++;
	}

	return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/**
 * @brief determine the type of font being used
 * 
 * @note only BDF or !BDF right now
 *
 * @param path path to font
 *
 * @return -1=failed to determine, FONT_BDF, FONT_SFT
 */
static int type(const char* path)
{
	size_t pl = strlen(path);

	if (pl < 4)
		return -1;

	if (strcicmp(path + pl - 4, ".bdf") == 0)
		return FONT_BDF;
	return FONT_SFT;
}

int font_load(Fontface* f, const char* path, double size)
{
	int t;

	if (!f || !path)
		return -1;

	memset(f, 0, sizeof(*f));

	t = type(path);
	if (t < 0)
		return -1;

	if (t == FONT_BDF) {
		f->bdf = calloc(BDF_GLYPHS_MAX, sizeof(*f->bdf));
		if (!f->bdf)
			return -1;
		return load_bdf(f, path);
	}

	if (size <= 0)
		return -1;

	return load_sft(f, path, size);
}

void font_free(Fontface* f)
{
	if (!f)
		return;

	if (f->font)
		sft_freefont(f->font);

	if (f->kind == FONT_BDF) {
		for (int i = 0; i < BDF_GLYPHS_MAX; i++)
			free(f->bdf[i].bmp);
		free(f->bdf);
	}

	memset(f, 0, sizeof(*f));
}
