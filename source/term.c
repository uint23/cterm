#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <grapheme.h>

#include "term.h"

static int csi_arg(Term* t, int i, int fallback);
static void csi_dispatch(Term* t, Caret* c, unsigned char ch);
static void csi_reset(Term* t);
static void erase_display(Term* t, Caret* c);
static void erase_line(Term* t, Caret* c);
static void reset_rune(Term* t, int x, int y);

/** TODO
 */
static int csi_arg(Term* t, int i, int fallback)
{
	if (i > t->csi_param)
		return fallback;
	if (t->csi_params[i] == 0)
		return fallback;

	return t->csi_params[i];
}

/** TODO
 */
static void csi_dispatch(Term* t, Caret* c, unsigned char ch)
{
	switch (ch) {
	case 'H': /* CUP - Cursor Position - ESC[row;colH */
	case 'f': { /* HVP - Horizontal and Vertical Postion - ESC[row;colf */
		int row = csi_arg(t, 0, 1) - 1;
		int col = csi_arg(t, 1, 1) - 1;

		if (row < 0)
			row = 0;
		if (col < 0)
			col = 0;
		if (row >= t->rows)
			row = t->rows - 1;
		if (col >= t->cols)
			col = t->cols - 1;

		c->y = row;
		c->x = col;
		break;
	}

	case 'J': /* ED - Erase in Display - ESC[nJ */
		erase_display(t, c);
		break;

	case 'K': /* EL - Erase in Line - ESC[nK */
		erase_line(t, c);
		break;

	case 'm': /* SGR - Select Graphics Rendition - ESC[nm, colours */
		/* TODO: colours */
		break;

	case 'A': { /* CUU - cursor up */
		int n = csi_arg(t, 0, 1);
		c->y -= n;
		if (c->y < 0)
			c->y = 0;
		break;
	}

	case 'B': { /* CUD - cursor down */
		int n = csi_arg(t, 0, 1);
		c->y += n;
		if (c->y >= t->rows)
			c->y = t->rows - 1;
		break;
	}

	case 'C': { /* CUF - cursor forward */
		int n = csi_arg(t, 0, 1);
		c->x += n;
		if (c->x >= t->cols)
			c->x = t->cols - 1;
		break;
	}

	case 'D': { /* CUB - cursor back */
			  int n = csi_arg(t, 0, 1);
			  c->x -= n;
			  if (c->x < 0)
				  c->x = 0;
			  break;
	}

	case 'G': { /* CHA - cursor horizontal absolute */
			  int col = csi_arg(t, 0, 1) - 1;

			  if (col < 0)
				  col = 0;
			  if (col >= t->cols)
				  col = t->cols - 1;

			  c->x = col;
			  break;
	}

	case 'd': { /* VPA - vertical position absolute */
			  int row = csi_arg(t, 0, 1) - 1;

			  if (row < 0)
				  row = 0;
			  if (row >= t->rows)
				  row = t->rows - 1;

			  c->y = row;
			  break;
	}

	/* TODO? unsupported */
	case 'h': /* set mode */
	case 'l': /* reset mode */
	case 'r': /* scrolling region */
	case 's': /* save cursor */
	case 'u': /* restore cursor */
		break;

	default: /* Unsupported CSI */
		break;
	}
}

/**
 * @brief erase CSI parameters
 *
 * @param t term instance to reset
 */
static void csi_reset(Term* t)
{
	for (int i = 0; i < CSI_PARAMS_MAX; i++)
		t->csi_params[i] = 0;
	t->csi_param = 0;
}

/** TODO
 */
static void erase_display(Term* t, Caret* c)
{
	int mode = csi_arg(t, 0, 0);

	/* mode 2: whole screen */
	if (mode == 2 || mode == 3) {
		for (int y = 0; y < t->rows; y++) {
			for (int x = 0; x < t->cols; x++)
				reset_rune(t, x, y);
		}
		return;
	}

	/* mode 0: cursor to end of screen */
	if (mode == 0) {
		for (int y = c->y; y < t->rows; y++) {
			int x0 = (y == c->y) ? c->x : 0;
			for (int x = x0; x < t->cols; x++)
				reset_rune(t, x, y);
		}
		return;
	}

	/* mode 1: start of screen to cursor */
	if (mode == 1) {
		for (int y = 0; y <= c->y; y++) {
			int x1 = (y == c->y) ? c->x : t->cols - 1;
			for (int x = 0; x <= x1; x++)
				reset_rune(t, x, y);
		}
	}
}

/** TODO
 */
static void erase_line(Term* t, Caret* c)
{
	for (int x = c->x; x < t->cols; x++)
		reset_rune(t, x, c->y);
}

/** TODO
 */
static void reset_rune(Term* t, int x, int y)
{
	Rune* r = &(RUNE(t, x, y));
	r->cp = ' ';
	r->fg = t->fg;
	r->bg = t->bg;
	r->dmg = true;
}

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

void term_damage_all(Term* t)
{
	for (int y = 0; y < t->rows; y++) {
		for (int x = 0; x < t->cols; x++)
			RUNE(t, x, y).dmg = true;
	}
}

void term_damage_rune(Term* t, int x, int y)
{
	if (x < 0 || y < 0 || x >= t->cols || y >= t->rows)
		return;
	RUNE(t, x, y).dmg = true;
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

		Rune* r = &RUNE(t, c->x, c->y);
		if (r->cp != cp || r->fg != t->fg || r->bg != t->bg) {
			r->cp = cp;
			r->fg = t->fg;
			r->bg = t->bg;
			r->dmg = true;
		}
		c->x++;
		if (c->x >= t->cols)
			term_putc(t, c, '\n');

		break;
	}
}

int term_resize(Term* t, Caret* c, int cols, int rows)
{
	/* old */
	Rune* or = t->runes;
	int ocols = t->cols;
	int orows = t->rows;

	if (cols < 1)
		cols = 1;
	if (rows < 1)
		rows = 1;

	/* new */
	Rune* nr = calloc(cols*rows, sizeof(*nr));
	if (!nr)
		return -1;

	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			Rune* r = &nr[y * cols + x];
			r->cp = ' ';
			r->fg = t->fg;
			r->bg = t->bg;
			r->dmg = true;
		}
	}

	if (or) {
		int copyrows = orows < rows ? orows : rows;
		int copycols = ocols < cols ? ocols : cols;

		for (int y = 0; y < copyrows; y++) {
			memcpy(
				&nr[y * cols], &or[y * ocols],
				sizeof(Rune) * copycols
			);
		}
	}

	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++)
			nr[y * cols + x].dmg = true;
	}

	free(or);

	t->runes = nr;
	t->cols = cols;
	t->rows = rows;

	if (c->x >= cols)
		c->x = cols - 1;
	if (c->y >= rows)
		c->y = rows - 1;
	if (c->x < 0)
		c->x = 0;
	if (c->y < 0)
		c->y = 0;

	return 0;
}

void term_scroll(Term* t)
{
	Rune* runes = t->runes;
	int cols = t->cols;
	int rows = t->rows;

	/* TODO: inefficient */
	memmove(runes, runes + cols, sizeof(Rune)*cols * (rows - 1));
	term_damage_all(t);

	for (int x = 0; x < t->cols; x++) {
		Rune* rune = &RUNE(t, x, t->rows - 1);
		rune->cp = ' ';
		rune->fg = t->fg;
		rune->bg = t->bg;
		rune->dmg = true;
	}
}

bool term_write(Term* t, Caret* c, const char* s, size_t n)
{
	bool damaged = false;

	for (size_t i = 0; i < n; i++) {
		unsigned char ch = (unsigned char)s[i];
		Caret old = *c;

		switch (t->state) {
		case TSTATE_NORMAL:
			if (ch == '\x1B') {
				t->state = TSTATE_ESC;
			}
			else {
				term_putc(t, c, ch);
			}
			break;

		case TSTATE_ESC:
			if (ch == '[') {
				csi_reset(t);
				t->state = TSTATE_CSI;
			}
			else {
				t->state = TSTATE_NORMAL;
			}
			break;

		case TSTATE_CSI:
			if (ch >= '0' && ch <= '9') {
				t->csi_params[t->csi_param] *= 10;
				t->csi_params[t->csi_param] += ch - '0';
			}
			else if (ch == ';') {
				if (t->csi_param + 1 < CSI_PARAMS_MAX)
					t->csi_param++;
			}
			else if (ch == '?' || ch == '>' || ch == '=') {
				/* private CSI marker */
			}
			else {
				csi_dispatch(t, c, ch);
				t->state = TSTATE_NORMAL;
			}
			break;
		}

		if (old.x != c->x || old.y != c->y)
			damaged = true;
	}

	for (int y = 0; y < t->rows; y++) {
		for (int x = 0; x < t->cols; x++) {
			if (RUNE(t, x, y).dmg)
				return true;
		}
	}

	return damaged;
}

