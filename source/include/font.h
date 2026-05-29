#ifndef FONT_H
#define FONT_H

#include <stdbool.h>

#include <schrift.h>

#define BDF_GLYPHS_MAX (65536)

typedef enum {
	FONT_BDF,
	FONT_SFT,
} FontKind;

typedef struct {
	bool          valid;
	uint8_t*      bmp;
	int           adv; /* advance */
	int           w;
	int           h;
} BdfFont;

typedef struct {
	FontKind      kind;

	SFT_Font*     font;
	SFT           sft;

	BdfFont*      bdf;

	double        size;
	double        asc;   /* ascent */
	double        dsc;   /* descent */
	int           cellw;
	int           cellh;
	bool          aa;    /* anti-aliasing */
} Fontface;

/**
 * @brief load font from path
 *
 * @param f font struct to write info to
 * @param path font path
 * @param size size of font
 *
 * @return 1=success, -1=failed
 */
int font_load(Fontface* f, const char* path, double size);

/**
 * @brief free a loaded font
 *
 * @param f font struct to free
 */
void font_free(Fontface* f);

#endif /* FONT_H */

