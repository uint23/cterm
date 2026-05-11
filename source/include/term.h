#ifndef TERM_H
#define TERM_H

#include <stdbool.h>
#include <stdint.h>

/* resolve 1D cell from 2D input */ 
#define CELL(t, x, y) ((t)->cells[(y) * (t)->cols + (x)])

struct Cell {
	uint32_t       cp;  /* codepoint */
	uint32_t       fg;
	uint32_t       bg;
	bool           dmg; /* cell damaged */
};

typedef struct {
	int            x;
	int            y;
} Cursor;

typedef struct {
	struct Cell*   cells;
	uint32_t       fg;
	uint32_t       bg;
	int            cols;
	int            rows;
	int            ptyfd;
} Term;

/**
 * @brief clear screen by filling cells with ' ' and reseting
 *        to default terminal foreground and background
 *
 * @param t terminal instance to scroll
 * @param c cursor instance to position
 */
void term_clear(Term* t, Cursor* c);

/**
 * @brief handle placing special and normal characters
 *
 * @param t terminal instance
 * @param c cursor instance
 * @param cp codepoint to place
 */
void term_putc(Term* t, Cursor* c, uint32_t cp);

/**
 * @brief scroll viewport down by one
 *
 * @param t terminal instance to scroll
 */
void term_scroll(Term* t);

#endif /* TERM_H */

