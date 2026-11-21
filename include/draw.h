#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>


/* draw a run of properties (look: vvvv) to screen */
void draw_rune(uint32_t* dst, int w, int h, int cl, int rw, int cw, int ch,
               uint32_t bg);

#endif /* DRAW_H */

