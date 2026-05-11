#include <stdbool.h>
#include <string.h>

#include "term.h"

void term_clear(Term* t, Cursor* c)
{
	for (int y = 0; y < t->rows; y++) {
		for (int x = 0; x < t->cols; x++) {
			struct Cell* cell = &CELL(t, x, y);
			cell->cp = ' ';
			cell->fg = t->fg;
			cell->bg = t->bg;
			cell->dmg = true;
		}
	}

	c->x = 0;
	c->y = 0;
}

void term_putc(Term* t, Cursor* c, uint32_t cp)
{
	switch (cp) {
	case '\r':
		c->x = 0;
		break;
	case '\n':
		c->x = 0;
		if (++c->y >= t->rows) {
			term_scroll(t);
			/* move to bottom */
			c->y = t->rows - 1;
		}
		break;
	case '\b':
		if (c->x > 0)
			c->x--;
		break;
	case '\t':
		/* move cursor to next tabstop */
		c->x = (c->x+8) & ~0x7;
		if (c->x >= t->cols)
			term_putc(t, c, '\n');
		break;
	default:
		if (cp < 32)
			break;

		CELL(t, c->x, c->y).cp = cp;
		CELL(t, c->x, c->y).fg = t->fg;
		CELL(t, c->x, c->y).bg = t->bg;
		CELL(t, c->x, c->y).dmg = true;
		c->x++;
		if (c->x >= t->cols)
			term_putc(t, c, '\n');

		break;
	}
}

void term_scroll(Term* t)
{
	struct Cell* cells = t->cells;
	int cols = t->cols;
	int rows = t->rows;

	/* TODO: inefficient */
	memmove(cells, cells + cols, sizeof(struct Cell)*cols * (rows - 1));

	for (int x = 0; x < t->cols; x++) {
		struct Cell* cell = &CELL(t, x, t->rows - 1);
		cell->cp = ' ';
		cell->fg = t->fg;
		cell->bg = t->bg;
		cell->dmg = true;
	}
}

