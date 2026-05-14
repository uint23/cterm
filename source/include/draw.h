#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>

#include "font.h"

#define GLYPH_CACHE_MAX 256

#define Glyph _Glyph /* Xrender defines the same */
typedef struct {
	bool           valid;
	int            w;
	int            h;
	int            lsb;
	int            yoff;
	uint8_t*       bmp;
} Glyph;

/**
 * @brief clear a buffer size wxh
 *
 * @param dst buffer to clear
 * @param w width of buffer
 * @param h height of buffer
 * @param col colour to clear buffer with
 */
void draw_clear(uint32_t* dst, int w, int h, uint32_t col);

/**
 * @brief fill the background of a rune
 *
 * @param dst buffer to draw to
 * @param w width of buffer
 * @param h height of buffer
 * @param cl rune column
 * @param rw rune row
 * @param cw rune width
 * @param ch rune height
 * @param bg rune background colour
 */
void draw_rune(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
               uint32_t bg);

/**
 * @brief fill the background of a rune
 *
 * @param dst buffer to draw to
 * @param w width of buffer
 * @param h height of buffer
 * @param cl rune column
 * @param rw rune row
 * @param cw rune width
 * @param ch rune height
 * @param fg rune foreground colour
 *
 * (TODO: runes)
 */
void draw_caret(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
                 uint32_t fg);

/**
 * @brief draw one codepoint to buffer
 *
 * @param face fontface to draw from
 * @param dst destination buffer
 * @param w buffer width
 * @param h buffer height
 * @param x x-position to draw at
 * @param baseline text baseline y-position
 * @param cp (unicode) codepoint to draw
 * @param fg foreground colour
 */
void draw_codepoint(Fontface* face, uint32_t* dst, int w, int h, int x,
                         int baseline, uint32_t cp, uint32_t fg);

#endif /* DRAW_H */

