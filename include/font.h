#ifndef FONT_H
#define FONT_H

#include <stdbool.h>
#include <stdint.h>

#define BDF_GLYPHS_MAX (65536)

typedef struct {
	bool          valid;
	uint8_t*      bmp;
	int           adv; /* advance */
	int           w;
	int           h;
	int           xoff;
	int           yoff;
} BdfFont;
typedef BdfFont BdfGlyph;

typedef struct {
	BdfFont*      bdf;

	double        asc;   /* ascent */
	double        dsc;   /* descent */
	int           cellw;
	int           cellh;
} Fontface;

/**
 * @brief load font from path
 *
 * @param f font struct to write info to
 * @param path font path
 * @return 0=success, -1=failed
 */
int font_load(Fontface* f, const char* path);

/**
 * @brief free a loaded font
 *
 * @param f font struct to free
 */
void font_free(Fontface* f);

#endif /* FONT_H */

