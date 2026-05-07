#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>

#include "font.h"

/* TODO: proper structs and faster drawing with damage for drawing shaepes*/

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
 * @brief fill the background of a cell
 *
 * @param dst buffer to draw to
 * @param w width of buffer
 * @param h height of buffer
 * @param cl cell column
 * @param rw cell row
 * @param cw cell width
 * @param ch cell height
 * @param bg cell background colour
 */
void draw_cell(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
               uint32_t bg);

/**
 * @brief fill the background of a cell
 *
 * @param dst buffer to draw to
 * @param w width of buffer
 * @param h height of buffer
 * @param cl cell column
 * @param rw cell row
 * @param cw cell width
 * @param ch cell height
 * @param fg cell foreground colour
 *
 * (TODO: glyphs)
 */
void draw_cursor(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
                 uint32_t fg);

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
void draw_codepoint(Font* f, uint32_t* dst, int w, int h, int x,
                         int baseline, uint32_t cp, uint32_t fg);

#endif /* DRAW_H */

