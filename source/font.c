#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"

static int load_bdf(Fontface* f, const char* path);
static int strcicmp(char const* a, char const* b);
static bool is_bdf(const char* path);

/* load a bdf font into `f`,  -1 on fail, 0 on success */
static int load_bdf(Fontface* f, const char* path)
{
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
				   multiple of 8 */
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

	if (f->cellw <= 0 || f->cellh <= 0) {
		font_free(f);
		return -1;
	}

	return 0;
}

/* case insensitive string compare 
   -1=(a<b), 0=(a==b), 1=(a>b) */
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

/* determine if font is BDF (by judging extension) */
static bool is_bdf(const char* path)
{
	size_t pl = strlen(path);

	if (pl < 4)
		return false;

	return strcicmp(path + pl - 4, ".bdf") == 0;
}

int font_load(Fontface* f, const char* path)
{
	int ret;

	if (!f || !path)
		return -1;

	memset(f, 0, sizeof(*f));

	if (!is_bdf(path))
		return -1;

	f->bdf = calloc(BDF_GLYPHS_MAX, sizeof(*f->bdf));
	if (!f->bdf)
		return -1;

	ret = load_bdf(f, path);
	if (ret < 0)
		font_free(f);
	return ret;
}

void font_free(Fontface* f)
{
	if (!f)
		return;

	if (f->bdf) {
		for (int i = 0; i < BDF_GLYPHS_MAX; i++)
			free(f->bdf[i].bmp);
		free(f->bdf);
	}

	memset(f, 0, sizeof(*f));
}

