#include <stdbool.h>
#include <string.h>

#include <grapheme.h>

#include "term.h"

void term_clear(Term* t, Caret* c)
{
	for (int y = 0; y < t->rows; y++) {
		for (int x = 0; x < t->cols; x++) {
			Rune* rune = &RUNE(t, x, y);
			rune->cp = ' ';
			rune->fg = t->fg;
			rune->bg = t->bg;
			rune->dmg = true;
		}
	}

	c->x = 0;
	c->y = 0;
}

void term_putc(Term* t, Caret* c, uint32_t cp)
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

		RUNE(t, c->x, c->y).cp = cp;
		RUNE(t, c->x, c->y).fg = t->fg;
		RUNE(t, c->x, c->y).bg = t->bg;
		RUNE(t, c->x, c->y).dmg = true;
		c->x++;
		if (c->x >= t->cols)
			term_putc(t, c, '\n');

		break;
	}
}

void term_scroll(Term* t)
{
	Rune* runes = t->runes;
	int cols = t->cols;
	int rows = t->rows;

	/* TODO: inefficient */
	memmove(runes, runes + cols, sizeof(Rune)*cols * (rows - 1));

	for (int x = 0; x < t->cols; x++) {
		Rune* rune = &RUNE(t, x, t->rows - 1);
		rune->cp = ' ';
		rune->fg = t->fg;
		rune->bg = t->bg;
		rune->dmg = true;
	}
}

void term_write(Term* t, Caret* c, const char* s, size_t n)
{
	size_t off = 0;

	while (off < n) {
		uint32_t cp;
		/* s+off: pointer to current position
		 * n-off: remaining bytes
		 */
		size_t len = grapheme_decode_utf8(s+off, n-off, &cp);

		if (len ==0)
			break;

		term_putc(t, c, cp);
		off += len;
	}
}

