#ifndef FONT_H
#define FONT_H

#include <stdbool.h>

#include <schrift.h>

typedef struct {
	SFT_Font*     font;
	SFT           sft;
	double        size;
	double        asc;   /* ascent */
	double        dsc;   /* descent */
	int           cellw;
	int           cellh;
	bool          aa;    /* anti-aliasing */
} Font;

/**
 * @brief load font from path
 *
 * @param f font struct to write info to
 * @param path font path
 * @param size size of font
 *
 * @return 1=success, -1=failed
 */
int font_load(Font* f, const char* path, double size);

/**
 * @brief free a loaded font
 *
 * @param f font struct to free
 */
void font_free(Font* f);

#endif /* FONT_H */

