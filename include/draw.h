#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>

#include "font.h"

/* clear a buffer size wxh */
void draw_clear(uint32_t* dst, int w, int h, uint32_t col);

/* draw a run of properties (look: vvvv) to screen */
void draw_rune(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
               uint32_t bg);

/* fill the background of a rune
   (TODO: runes) */
void draw_caret(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
                 uint32_t fg);

/* draw one codepoint to buffer */
void draw_codepoint(Fontface* face, uint32_t* dst, int w, int h, int x,
                    int baseline, uint32_t cp, uint32_t fg);

#endif /* DRAW_H */

