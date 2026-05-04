#ifndef FONT_H
#define FONT_H

#include <stdbool.h>

#include <schrift.h>

typedef struct {
	SFT_Font*     font;
	SFT           sft;
	double        size;
	double        asc; /* ascent */
	double        dsc; /* descent */
	bool          aa;  /* anti-aliasing */
} Font;

/**
 * @brief load font from path
 *
 * @param f font struct to write info to
 * @param path font path
 * @param size size of font
 */
int font_load(Font* f, const char* path, double size);

/**
 * @brief free a loaded font
 *
 * @param f font struct to free
 */
void font_free(Font* f);

/**
 * @brief draw one codepoint to buffer
 *
 * @param f font to draw from
 * @param dst destination buffer
 * @param w buffer width
 * @param h buffer height
 * @param x x-position to draw at
 * @param baseline text baseline y-position
 * @param cp (unicode) codepoint to draw
 * @param fg foreground colour
 */
void font_draw_codepoint(Font* f, uint32_t* dst, int w, int h, int x,
                         int baseline, uint32_t cp, uint32_t fg);

#endif /* FONT_H */

