#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "schrift.h"

static int strcicmp(char const* a, char const* b);
static int type(const char* path);

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
 * @return -1=failed to determine, 0=BDF, 1=!BDF
 */
static int type(const char* path)
{
	size_t pl = strlen(path);

	if (pl < 3)
		return -1;

	char ext[4];
	memcpy(ext, path + pl - 3, 3);
	ext[3] = '\0';

	if (strcicmp(ext, "bdf") == 0)
		return 0; /* BDF */

	return 1; /* not BDF */
}

int font_load(Fontface* f, const char* path, double size)
{
	SFT_LMetrics lm;
	SFT_Glyph glyph;
	SFT_GMetrics gm;

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

void font_free(Fontface* f)
{
	if (!f)
		return;
	if (f->font)
		sft_freefont(f->font);
	memset(f, 0, sizeof(*f));
}

