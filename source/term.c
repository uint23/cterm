#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <grapheme.h>

#include "term.h"
#include "utils.h"

static int csi_arg(Term* t, int i, int fallback);
static void csi_dispatch(Term* t, Caret* c, unsigned char ch);
static void csi_reset(Term* t);
static void erase_display(Term* t, Caret* c);
static void erase_line(Term* t, Caret* c);
static void reset_rune(Term* t, int x, int y);
static void sgr(Term* t);
static void utf8_flush(Term* t, Caret* c);
static void utf8_putc(Term* t, Caret* c, unsigned char ch);

static const uint32_t ansi_colours[16] = { /* TODO */
	0xff000000, 0xff0000cd, 0xff00cd00, 0xff00cdcd,
	0xffee0000, 0xffcd00cd, 0xffcdcd00, 0xffe5e5e5,
	0xff7f7f7f, 0xff0000ff, 0xff00ff00, 0xff00ffff,
	0xffff5c5c, 0xffff00ff, 0xffffff00, 0xffffffff,
};

/** TODO
 */
static int csi_arg(Term* t, int i, int fallback)
{
	if (i > t->csi_idx)
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
		sgr(t);
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
	t->csi_idx = 0;
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

/** TODO
 */
static void sgr(Term* t)
{
	for (int i = 0; i <= t->csi_idx; i++) {
		int p = t->csi_params[i];

		if (p == 0) {
			t->fg = TERM_DEFAULT_FG;
			t->bg = TERM_DEFAULT_BG;
		}
		else if (p >= 30 && p <= 37)
			t->fg = ansi_colours[p - 30];
		else if (p >= 40 && p <= 47)
			t->bg = ansi_colours[p - 40];
		else if (p >= 90 && p <= 97)
			t->fg = ansi_colours[p - 90 + 8];
		else if (p >= 100 && p <= 107)
			t->bg = ansi_colours[p - 100 + 8];
		else if (p == 39)
			t->fg = TERM_DEFAULT_FG;
		else if (p == 49)
			t->bg = TERM_DEFAULT_BG;
		else if ((p == 38 || p == 48) &&
		         i + 4 <= t->csi_idx && t->csi_params[i + 1] == 2) {
			uint32_t col = rgba(
				t->csi_params[i + 2],
				t->csi_params[i + 3],
				t->csi_params[i + 4], 255
			);
			if (p == 38)
				t->fg = col;
			else
				t->bg = col;
			i += 4;
		}
	}
}

/** TODO
 */
static void utf8_flush(Term* t, Caret* c)
{
	if (t->utf8_len == 0)
		return;
	term_putc(t, c, GRAPHEME_INVALID_CODEPOINT);
	t->utf8_len = 0;
}

/** TODO
 */
static void utf8_putc(Term* t, Caret* c, unsigned char ch)
{
	uint_least32_t cp;
	size_t ret;

	if (ch < 0x80) {
		utf8_flush(t, c);
		term_putc(t, c, ch);
		return;
	}

	if (t->utf8_len == sizeof(t->utf8))
		utf8_flush(t, c);

	t->utf8[t->utf8_len++] = (char)ch;
	for (;;) {
		ret = grapheme_decode_utf8(t->utf8, t->utf8_len, &cp);
		if (ret > t->utf8_len)
			return;
		if (ret == 0) {
			t->utf8_len = 0;
			return;
		}

		term_putc(t, c, (uint32_t)cp);
		t->utf8_len -= ret;
		if (t->utf8_len == 0)
			return;
		memmove(t->utf8, t->utf8 + ret, t->utf8_len);
	}
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
				utf8_flush(t, c);
				t->state = TSTATE_ESC;
			}
			else {
				utf8_putc(t, c, ch);
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
				t->csi_params[t->csi_idx] *= 10;
				t->csi_params[t->csi_idx] += ch - '0';
			}
			else if (ch == ';') {
				if (t->csi_idx + 1 < CSI_PARAMS_MAX)
					t->csi_idx++;
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

