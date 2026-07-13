#ifndef MAUS_FONT_H
#define MAUS_FONT_H

#include <stdint.h>
#include <stdbool.h>

#include "maus.h"

#define MAUS_BDF_GLYPHS_MAX 256

typedef struct {
    uint8_t*    bmp;
    uint16_t    w;
    uint16_t    h;
    int16_t     xoff;
    int16_t     yoff;
    uint16_t    adv;
    bool        valid;
} MausGlyph;

typedef struct {
    MausGlyph glyphs[MAUS_BDF_GLYPHS_MAX];
    int32_t     asc;
    int32_t     dsc;
    int32_t     cellh;
    int32_t     cellw;
} MausFont;

/* Loops through text passing parameters to glyph drawer */
void maus_draw_text(Maus* mw, MausFont* font, int32_t x, int32_t y,
                    const char* text, MausColor col);

/* load a bdf font into MausFont. returns NULL on fail */
MausFont* maus_font_load(const char* path);

/* release all allocations made with a MausFont */
void maus_font_free(MausFont* font);

#endif /* MAUS_FONT_H */

