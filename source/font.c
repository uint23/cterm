#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "schrift.h"

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

