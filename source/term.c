#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <grapheme.h>

#include "../config.h"
#include "term.h"
#include "utils.h"

static int csi_arg(Term* t, int i, int fallback);
static void alt_screen(Term* t, Caret* c, bool on);
static void chars_insert(Term* t, Caret* c, int n);
static void chars_delete(Term* t, Caret* c, int n);
static uint32_t acs_map(unsigned char ch);
static void csi_dispatch(Term* t, Caret* c, unsigned char ch);
static void csi_reset(Term* t);
static int cp_width(uint32_t cp);
static void erase_display(Term* t, Caret* c);
static void erase_line(Term* t, Caret* c);
static void reset_rune(Term* t, int x, int y);
static void rune_prepare(Term* t, int x, int y);
static void scroll_down(Term* t, int top, int bot, int n);
static void scroll_up(Term* t, int top, int bot, int n);
static void sgr(Term* t);
static void utf8_flush(Term* t, Caret* c);
static void utf8_putc(Term* t, Caret* c, unsigned char ch);

/** TODO
 */
static uint32_t acs_map(unsigned char ch)
{
	switch (ch) {
	case '`': return 0x25c6; /* ◆ */ case 'a': return 0x2592; /* ▒ */
	case 'f': return 0x00b0; /* ° */ case 'g': return 0x00b1; /* ± */
	case 'j': return 0x2518; /* ┘ */ case 'k': return 0x2510; /* ┐ */
	case 'l': return 0x250c; /* ┌ */ case 'm': return 0x2514; /* └ */
	case 'n': return 0x253c; /* ┼ */ case 'q': return 0x2500; /* ─ */
	case 't': return 0x251c; /* ├ */ case 'u': return 0x2524; /* ┤ */
	case 'v': return 0x2534; /* ┴ */ case 'w': return 0x252c; /* ┬ */
	case 'x': return 0x2502; /* │ */ case 'y': return 0x2264; /* ≤ */
	case 'z': return 0x2265; /* ≥ */ case '{': return 0x03c0; /* π */
	case '|': return 0x2260; /* ≠ */ case '}': return 0x00a3; /* £ */
	case '~': return 0x00b7; /* · */ default:  return ch;
	}
}

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
static void alt_screen(Term* t, Caret* c, bool on)
{
	Rune* tmp;

	if (on) {
		if (t->alt)
			return;
		tmp = calloc(t->cols*t->rows, sizeof(*tmp));
		if (!tmp)
			return;
		t->alt = t->runes;
		t->runes = tmp;
		term_clear(t, c);
		return;
	}

	if (!t->alt)
		return;
	free(t->runes);
	t->runes = t->alt;
	t->alt = NULL;
	term_damage_all(t);
}

/** TODO
 */
static void chars_insert(Term* t, Caret* c, int n)
{
	int cols = t->cols - c->x;
	Rune* line = &RUNE(t, 0, c->y);

	if (n > cols)
		n = cols;
	memmove(&line[c->x + n], &line[c->x], sizeof(Rune) * (cols - n));
	for (int x = c->x; x < c->x + n; x++)
		reset_rune(t, x, c->y);
	for (int x = c->x + n; x < t->cols; x++)
		RUNE(t, x, c->y).dmg = true;
}

/** TODO
 */
static void chars_delete(Term* t, Caret* c, int n)
{
	int cols = t->cols - c->x;
	Rune* line = &RUNE(t, 0, c->y);

	if (n > cols)
		n = cols;
	memmove(&line[c->x], &line[c->x + n], sizeof(Rune) * (cols - n));
	for (int x = t->cols - n; x < t->cols; x++)
		reset_rune(t, x, c->y);
	for (int x = c->x; x < t->cols - n; x++)
		RUNE(t, x, c->y).dmg = true;
}

/** TODO
 */
static void csi_dispatch(Term* t, Caret* c, unsigned char ch)
{
	switch (ch) {
	case '@': /* ICH - Insert Character */
		t->wrapnext = false;
		chars_insert(t, c, csi_arg(t, 0, 1));
		break;

	case 'H': /* CUP - Cursor Position - ESC[row;colH */
	case 'f': { /* HVP - Horizontal and Vertical Postion - ESC[row;colf */
		t->wrapnext = false;
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

	case 'P': /* DCH - Delete Character */
		t->wrapnext = false;
		chars_delete(t, c, csi_arg(t, 0, 1));
		break;

	case 'X': { /* ECH - Erase Character */
		int n = csi_arg(t, 0, 1);
		for (int x = c->x; x < c->x + n && x < t->cols; x++)
			reset_rune(t, x, c->y);
		break;
	}

	case 'm': /* SGR - Select Graphics Rendition - ESC[nm, colours */
		sgr(t);
		break;

	case 'A': { /* CUU - cursor up */
		t->wrapnext = false;
		int n = csi_arg(t, 0, 1);
		c->y -= n;
		if (c->y < 0)
			c->y = 0;
		break;
	}

	case 'B': { /* CUD - cursor down */
		t->wrapnext = false;
		int n = csi_arg(t, 0, 1);
		c->y += n;
		if (c->y >= t->rows)
			c->y = t->rows - 1;
		break;
	}

	case 'C': { /* CUF - cursor forward */
		t->wrapnext = false;
		int n = csi_arg(t, 0, 1);
		c->x += n;
		if (c->x >= t->cols)
			c->x = t->cols - 1;
		break;
	}

	case 'D': { /* CUB - cursor back */
			  t->wrapnext = false;
			  int n = csi_arg(t, 0, 1);
			  c->x -= n;
			  if (c->x < 0)
				  c->x = 0;
			  break;
	}

	case 'G': { /* CHA - cursor horizontal absolute */
			  t->wrapnext = false;
			  int col = csi_arg(t, 0, 1) - 1;

			  if (col < 0)
				  col = 0;
			  if (col >= t->cols)
				  col = t->cols - 1;

			  c->x = col;
			  break;
	}

	case 'd': { /* VPA - vertical position absolute */
			  t->wrapnext = false;
			  int row = csi_arg(t, 0, 1) - 1;

			  if (row < 0)
				  row = 0;
			  if (row >= t->rows)
				  row = t->rows - 1;

			  c->y = row;
			  break;
	}

	case 'L': /* IL - Insert Line */
		t->wrapnext = false;
		if (c->y >= t->scroll_top && c->y <= t->scroll_bot)
			scroll_down(t, c->y, t->scroll_bot, csi_arg(t, 0, 1));
		break;

	case 'M': /* DL - Delete Line */
		t->wrapnext = false;
		if (c->y >= t->scroll_top && c->y <= t->scroll_bot)
			scroll_up(t, c->y, t->scroll_bot, csi_arg(t, 0, 1));
		break;

	case 'S': /* SU - Scroll Up */
		t->wrapnext = false;
		scroll_up(t, t->scroll_top, t->scroll_bot, csi_arg(t, 0, 1));
		break;

	case 'T': /* SD - Scroll Down */
		t->wrapnext = false;
		scroll_down(t, t->scroll_top, t->scroll_bot, csi_arg(t, 0, 1));
		break;

	case 'h': /* set mode */
	case 'l': { /* reset mode */
		bool on = ch == 'h';
		for (int i = 0; i <= t->csi_idx; i++) {
			int p = t->csi_params[i];
			if (p == 25)
				t->cursor_visible = on;
			if (p == 1048) {
				if (on)
					t->saved = *c;
				else
					*c = t->saved;
			}
			if (p == 47 || p == 1047 || p == 1049) {
				if (on && p == 1049)
					t->saved = *c;
				alt_screen(t, c, on);
				if (!on && p == 1049)
					*c = t->saved;
			}
		}
		break;
	}
	case 'r': { /* DECSTBM - scrolling region */
		t->wrapnext = false;
		int top = csi_arg(t, 0, 1) - 1;
		int bot = csi_arg(t, 1, t->rows) - 1;
		if (top < 0)
			top = 0;
		if (bot >= t->rows)
			bot = t->rows - 1;
		if (top < bot) {
			t->scroll_top = top;
			t->scroll_bot = bot;
			c->x = 0;
			c->y = 0;
		}
		break;
	}
	case 's': t->saved = *c; break; /* save cursor */
	case 'u': t->wrapnext = false; *c = t->saved; break; /* restore cursor */

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
static int cp_width(uint32_t cp)
{
	int w = wcwidth((wchar_t)cp);

	if (w < 0)
		return 1;
	if (w > 2)
		return 2;
	return w;
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
	int mode = csi_arg(t, 0, 0);
	int from = mode == 0 ? c->x : 0;
	int to = mode == 1 ? c->x : t->cols - 1;

	for (int x = from; x <= to; x++)
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
	r->attr = t->attr;
	r->width = 1;
	r->dmg = true;
}

/** TODO
 */
static void rune_prepare(Term* t, int x, int y)
{
	if (x > 0 && RUNE(t, x, y).width == 0)
		reset_rune(t, x - 1, y);
	if (RUNE(t, x, y).width == 2 && x + 1 < t->cols)
		reset_rune(t, x + 1, y);
}

/** TODO
 */
static void scroll_down(Term* t, int top, int bot, int n)
{
	int cols = t->cols;
	int rows = bot - top + 1;

	if (n > rows)
		n = rows;
	memmove(
		&RUNE(t, 0, top + n), &RUNE(t, 0, top),
		sizeof(Rune) * cols * (rows - n)
	);
	for (int y = top; y < top + n; y++) {
		for (int x = 0; x < cols; x++)
			reset_rune(t, x, y);
	}
	term_damage_all(t);
}

/** TODO
 */
static void scroll_up(Term* t, int top, int bot, int n)
{
	int cols = t->cols;
	int rows = bot - top + 1;

	if (n > rows)
		n = rows;
	memmove(
		&RUNE(t, 0, top), &RUNE(t, 0, top + n),
		sizeof(Rune) * cols * (rows - n)
	);
	for (int y = bot - n + 1; y <= bot; y++) {
		for (int x = 0; x < cols; x++)
			reset_rune(t, x, y);
	}
	term_damage_all(t);
}

/** TODO
 */
static void sgr(Term* t)
{
	for (int i = 0; i <= t->csi_idx; i++) {
		int p = t->csi_params[i];

		if (p == 0) {
			t->fg = default_fg;
			t->bg = default_bg;
			t->attr = 0;
		}
		else if (p == 7)
			t->attr |= TERM_ATTR_REVERSE;
		else if (p == 27)
			t->attr &= ~TERM_ATTR_REVERSE; /* turn off reverse attribute bit */
		else if (p >= 30 && p <= 37)
			t->fg = color_table[p - 30];
		else if (p >= 40 && p <= 47)
			t->bg = color_table[p - 40];
		else if (p >= 90 && p <= 97)
			t->fg = color_table[p - 90 + 8];
		else if (p >= 100 && p <= 107)
			t->bg = color_table[p - 100 + 8];
		else if (p == 39)
			t->fg = default_fg;
		else if (p == 49)
			t->bg = default_bg;
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
		term_putc(t, c, t->acs[t->charset] ? acs_map(ch) : ch);
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
			rune->attr = t->attr;
			rune->width = 1;
			rune->dmg = true;
		}
	}

	c->x = 0;
	c->y = 0;
	t->wrapnext = false;
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
		t->wrapnext = false;
		break;
	case '\n':
		c->x = 0;
		t->wrapnext = false;
		if (c->y == t->scroll_bot) {
			term_scroll(t);
		}
		else if (c->y < t->rows - 1)
			c->y++;
		break;
	case '\b':
		t->wrapnext = false;
		if (c->x > 0)
			c->x--;
		break;
	case '\t':
		t->wrapnext = false;
		/* move cursor to next tabstop */
		c->x = (c->x+8) & ~0x7;
		if (c->x >= t->cols)
			term_putc(t, c, '\n');
		break;
	default: {
		int w;

		if (cp < 32)
			break;
		w = cp_width(cp);
		if (w == 0)
			break;
		if (w == 2 && t->cols < 2)
			w = 1;

		if (t->wrapnext) {
			term_putc(t, c, '\n');
			t->wrapnext = false;
		}
		if (w == 2 && c->x == t->cols - 1)
			term_putc(t, c, '\n');

		rune_prepare(t, c->x, c->y);
		Rune* r = &RUNE(t, c->x, c->y);
		if (r->cp != cp || r->fg != t->fg || r->bg != t->bg ||
		    r->attr != t->attr || r->width != w) {
			r->cp = cp;
			r->fg = t->fg;
			r->bg = t->bg;
			r->attr = t->attr;
			r->width = w;
			r->dmg = true;
		}
		if (w == 2) {
			Rune* spacer = &RUNE(t, c->x + 1, c->y);
			spacer->cp = ' ';
			spacer->fg = t->fg;
			spacer->bg = t->bg;
			spacer->attr = t->attr;
			spacer->width = 0;
			spacer->dmg = true;
		}

		if (c->x + w >= t->cols)
			t->wrapnext = true;
		else
			c->x += w;

		break;
	}
	}
}

int term_resize(Term* t, Caret* c, int cols, int rows)
{
	/* old */
	Rune* or = t->runes;
	int ocols = t->cols;
	int orows = t->rows;

	if (t->alt) {
		free(t->runes);
		t->runes = t->alt;
		t->alt = NULL;
		or = t->runes;
	}

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
			r->attr = t->attr;
			r->width = 1;
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
	t->scroll_top = 0;
	t->scroll_bot = rows - 1;

	if (c->x >= cols)
		c->x = cols - 1;
	if (c->y >= rows)
		c->y = rows - 1;
	if (c->x < 0)
		c->x = 0;
	if (c->y < 0)
		c->y = 0;
	t->wrapnext = false;
	if (t->saved.x >= cols)
		t->saved.x = cols - 1;
	if (t->saved.y >= rows)
		t->saved.y = rows - 1;

	return 0;
}

void term_scroll(Term* t)
{
	scroll_up(t, t->scroll_top, t->scroll_bot, 1);
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
			else if (ch == '\x0E') {
				t->charset = 1;
			}
			else if (ch == '\x0F') {
				t->charset = 0;
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
			else if (ch == ']') {
				t->state = TSTATE_OSC;
			}
			else if (ch == '(') {
				t->charset_target = 0;
				t->state = TSTATE_CHARSET;
			}
			else if (ch == ')') {
				t->charset_target = 1;
				t->state = TSTATE_CHARSET;
			}
			else if (ch == '*' || ch == '+') {
				t->state = TSTATE_CHARSET_SKIP;
			}
			else if (ch == '7') {
				t->saved = *c;
				t->state = TSTATE_NORMAL;
			}
			else if (ch == '8') {
				*c = t->saved;
				t->state = TSTATE_NORMAL;
			}
			else if (ch == 'M') {
				t->wrapnext = false;
				if (c->y == t->scroll_top)
					scroll_down(t, t->scroll_top, t->scroll_bot, 1);
				else if (c->y > 0)
					c->y--;
				t->state = TSTATE_NORMAL;
			}
			else if (ch == 'D' || ch == 'E') {
				t->wrapnext = false;
				if (ch == 'E')
					c->x = 0;
				if (c->y == t->scroll_bot)
					scroll_up(t, t->scroll_top, t->scroll_bot, 1);
				else if (c->y < t->rows - 1)
					c->y++;
				t->state = TSTATE_NORMAL;
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

		case TSTATE_OSC:
			if (ch == '\a')
				t->state = TSTATE_NORMAL;
			else if (ch == '\x1B')
				t->state = TSTATE_OSC_ESC;
			break;

		case TSTATE_OSC_ESC:
			t->state = ch == '\\' ? TSTATE_NORMAL : TSTATE_OSC;
			break;

		case TSTATE_CHARSET:
			t->acs[t->charset_target] = ch == '0';
			t->state = TSTATE_NORMAL;
			break;

		case TSTATE_CHARSET_SKIP:
			t->state = TSTATE_NORMAL;
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
