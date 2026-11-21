#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <uleutf.h>

#include "../config.h"
#include "term.h"
#include "utils.h"

#define DCS_MAX_SIZE (8 * 1024 * 1024)

static uint32_t acs_map(unsigned char ch);
static void alt_screen(Term* t, Caret* c, bool on);
static void chars_shift(Term* t, Caret* c, int n, bool insert);
static int csi_arg(Term* t, int i, int fallback);
static void csi_dispatch(Term* t, Caret* c, unsigned char ch);
static void csi_reset(Term* t);
static void dcs_append(Term* t, unsigned char ch);
static bool dcs_finish(Term* t, Caret* c);
static void erase_display(Term* t, Caret* c);
static void erase_line(Term* t, Caret* c);
static void reset_rune(Term* t, int x, int y);
static void rune_prepare(Term* t, int x, int y);
static void scroll_region(Term* t, int top, int bot, int lines);
static void sgr(Term* t);
static void sixel_add(Term* t, Sixel img, Caret pos);
static void sixel_clear(SixelLayer* layer);
static void sixel_erase(Term* t, int x, int y, int w, int h);
static bool sixel_overlaps(const Term* t, const SixelView* view,
                           int x, int y, int w, int h);
static void sixel_remove(SixelLayer* layer, size_t i);
static int sixel_rows(const Term* t, const Sixel* img);
static void sixel_scroll(Term* t, int top, int bot, int lines);

/* TODO */
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

/* TODO */
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
		t->alt_sixels = t->sixels;
		t->sixels = (SixelLayer){ 0 };
		term_clear(t, c);
		return;
	}

	if (!t->alt)
		return;
	sixel_clear(&t->sixels);
	free(t->runes);
	t->runes = t->alt;
	t->alt = NULL;
	t->sixels = t->alt_sixels;
	t->alt_sixels = (SixelLayer){ 0 };
	term_damage_all(t);
}

static void chars_shift(Term* t, Caret* c, int n, bool insert)
{
	int cols = t->cols - c->x;
	Rune* line = &RUNE(t, 0, c->y);
	if (n > cols)
		n = cols;
	sixel_erase(t, c->x, c->y, cols, 1);
	int src = insert ? c->x : c->x + n;
	int dst = insert ? c->x + n : c->x;
	memmove(&line[dst], &line[src], sizeof(Rune) * (cols - n));
	int clear = insert ? c->x : t->cols - n;
	for (int x = clear; x < clear + n; x++)
		reset_rune(t, x, c->y);
	for (int x = c->x; x < t->cols; x++)
		RUNE(t, x, c->y).dmg = true;
}

/* TODO */
static int csi_arg(Term* t, int i, int fallback)
{
	if (i > t->csi_idx)
		return fallback;
	if (t->csi_params[i] == 0)
		return fallback;

	return t->csi_params[i];
}

/* TODO */
static void csi_dispatch(Term* t, Caret* c, unsigned char ch)
{
	switch (ch) {
	case '@': /* ICH - Insert Character */
		t->wrapnext = false;
		chars_shift(t, c, csi_arg(t, 0, 1), true);
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
		chars_shift(t, c, csi_arg(t, 0, 1), false);
		break;

	case 'X': { /* ECH - Erase Character */
		int n = csi_arg(t, 0, 1);
		sixel_erase(t, c->x, c->y, n, 1);
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
			scroll_region(t, c->y, t->scroll_bot, csi_arg(t, 0, 1));
		break;

	case 'M': /* DL - Delete Line */
		t->wrapnext = false;
		if (c->y >= t->scroll_top && c->y <= t->scroll_bot)
			scroll_region(t, c->y, t->scroll_bot, -csi_arg(t, 0, 1));
		break;

	case 'S': /* SU - Scroll Up */
		t->wrapnext = false;
		scroll_region(t, t->scroll_top, t->scroll_bot, -csi_arg(t, 0, 1));
		break;

	case 'T': /* SD - Scroll Down */
		t->wrapnext = false;
		scroll_region(t, t->scroll_top, t->scroll_bot, csi_arg(t, 0, 1));
		break;

	case 'h': /* set mode */
	case 'l': { /* reset mode */
		bool on = ch == 'h';
		for (int i = 0; i <= t->csi_idx; i++) {
			int p = t->csi_params[i];
			if (p == 25)
				t->cursor_visible = on;
			if (p == 2004)
				t->bracketed_paste = on;
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

/* erase CSI parameters */
static void csi_reset(Term* t)
{
	for (int i = 0; i < CSI_PARAMS_MAX; i++)
		t->csi_params[i] = 0;
	t->csi_idx = 0;
}

/* buffer a SIXEL DCS payload */
static void dcs_append(Term* t, unsigned char ch)
{
	if (!t->dcs_sixel)
		return;
	if (t->dcs_len >= DCS_MAX_SIZE) {
		t->dcs_sixel = false;
		return;
	}
	if (t->dcs_len == t->dcs_cap) {
		size_t cap = t->dcs_cap ? t->dcs_cap * 2 : 4096;
		if (cap > DCS_MAX_SIZE)
			cap = DCS_MAX_SIZE;

		char* data = realloc(t->dcs_data, cap);
		if (!data) {
			t->dcs_sixel = false;
			return;
		}
		t->dcs_data = data;
		t->dcs_cap = cap;
	}

	t->dcs_data[t->dcs_len++] = ch;
}

/* decode a complete SIXEL DCS */
static bool dcs_finish(Term* t, Caret* c)
{
	bool decoded = false;

	if (t->dcs_sixel && t->dcs_len) {
		Sixel img = sixel_decode(t->dcs_data, t->dcs_len);
		if (img.pixels) {
			int rows = sixel_rows(t, &img);
			sixel_add(t, img, *c);
			for (int row = 0; row < rows; row++)
				term_putc(t, c, '\n');
			term_damage_all(t);
			decoded = true;
		}
	}

	t->dcs_len = 0;
	t->dcs_sixel = false;
	return decoded;
}

/* TODO */
static void erase_display(Term* t, Caret* c)
{
	int mode = csi_arg(t, 0, 0);
	if (mode < 0 || mode > 3)
		return;
	if (mode >= 2) {
		sixel_clear(&t->sixels);
		t->sixels_dirty = true;
	}
	for (int y = 0; y < t->rows; y++) {
		if ((mode == 0 && y < c->y) || (mode == 1 && y > c->y))
			continue;
		int from = mode == 0 && y == c->y ? c->x : 0;
		int to = mode == 1 && y == c->y ? c->x : t->cols - 1;
		if (mode < 2)
			sixel_erase(t, from, y, to - from + 1, 1);
		for (int x = from; x <= to; x++)
			reset_rune(t, x, y);
	}
}

/* TODO */
static void erase_line(Term* t, Caret* c)
{
	int mode = csi_arg(t, 0, 0);
	if (mode < 0 || mode > 2)
		return;
	int from = mode == 0 ? c->x : 0;
	int to = mode == 1 ? c->x : t->cols - 1;
	sixel_erase(t, from, c->y, to - from + 1, 1);

	for (int x = from; x <= to; x++)
		reset_rune(t, x, c->y);
}

/* TODO */
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

/* TODO */
static void rune_prepare(Term* t, int x, int y)
{
	if (x > 0 && RUNE(t, x, y).width == 0)
		reset_rune(t, x - 1, y);
	if (RUNE(t, x, y).width == 2 && x + 1 < t->cols)
		reset_rune(t, x + 1, y);
}

/* scroll cells and their images through the same region */
static void scroll_region(Term* t, int top, int bot, int lines)
{
	int rows = bot - top + 1;
	int n = lines > 0 ? lines : -lines;
	if (n > rows)
		n = rows;
	if (!n)
		return;
	if (n < rows) {
		int src = lines > 0 ? top : top + n;
		int dst = lines > 0 ? top + n : top;
		memmove(&RUNE(t, 0, dst), &RUNE(t, 0, src),
		        sizeof(Rune) * t->cols * (rows - n));
	}
	int clear = lines > 0 ? top : bot - n + 1;
	for (int y = clear; y < clear + n; y++) {
		for (int x = 0; x < t->cols; x++)
			reset_rune(t, x, y);
	}
	sixel_scroll(t, top, bot, lines > 0 ? n : -n);
	term_damage_all(t);
}

/* TODO */
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

static void sixel_add(Term* t, Sixel img, Caret pos)
{
	SixelLayer* layer = &t->sixels;
	size_t pixels = (size_t)img.width * img.height;
	while (layer->count && (layer->count == SIXEL_MAX_IMAGES ||
	       layer->pixels > SIXEL_MAX_PIXELS - pixels))
		sixel_remove(layer, 0);
	layer->views[layer->count++] = (SixelView){ img, pos };
	layer->pixels += pixels;
}

static void sixel_clear(SixelLayer* layer)
{
	for (size_t i = 0; i < layer->count; i++)
		sixel_free(&layer->views[i].image);
	*layer = (SixelLayer){ 0 };
}

static void sixel_erase(Term* t, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
		return;
	bool erased = false;
	SixelLayer* layer = &t->sixels;
	for (size_t i = 0; i < layer->count;) {
		if (sixel_overlaps(t, &layer->views[i], x, y, w, h)) {
			sixel_remove(layer, i);
			erased = true;
		}
		else
			i++;
	}
	if (erased)
		term_damage_all(t);
}

static bool sixel_overlaps(const Term* t, const SixelView* view,
                           int x, int y, int w, int h)
{
	int cw = t->cellw > 0 ? t->cellw : 1;
	int cols = (view->image.width - 1) / cw + 1;
	int rows = sixel_rows(t, &view->image);
	return view->pos.x < x + w && view->pos.x + cols > x &&
	       view->pos.y < y + h && view->pos.y + rows > y;
}

static void sixel_remove(SixelLayer* layer, size_t i)
{
	SixelView* view = &layer->views[i];
	layer->pixels -= (size_t)view->image.width * view->image.height;
	sixel_free(&view->image);
	layer->count--;
	memmove(view, view + 1, (layer->count - i) * sizeof(*view));
}

/* image height rounded up to terminal rows */
static int sixel_rows(const Term* t, const Sixel* img)
{
	int cellh = t->cellh > 0 ? t->cellh : 1;
	return (img->height - 1) / cellh + 1;
}

/* scroll images in the region; discard those cut by a partial region */
static void sixel_scroll(Term* t, int top, int bot, int lines)
{
	SixelLayer* layer = &t->sixels;
	bool partial = top != 0 || bot != t->rows - 1;
	for (size_t i = 0; i < layer->count;) {
		SixelView* view = &layer->views[i];
		int rows = sixel_rows(t, &view->image);
		int y = view->pos.y;
		if (partial && (y + rows <= top || y > bot)) {
			i++;
			continue;
		}
		view->pos.y += lines;
		if ((partial && (y < top || y + rows > bot + 1 ||
		                  view->pos.y < top || view->pos.y + rows > bot + 1)) ||
		    (!partial && (view->pos.y >= t->rows ||
		                  view->pos.y + rows <= 0 || view->pos.x >= t->cols))) {
			sixel_remove(layer, i);
			continue;
		}
		i++;
	}
}

void term_clear(Term* t, Caret* c)
{
	sixel_clear(&t->sixels);
	t->sixels_dirty = true;
	for (int y = 0; y < t->rows; y++) {
		for (int x = 0; x < t->cols; x++)
			reset_rune(t, x, y);
	}

	c->x = 0;
	c->y = 0;
	t->wrapnext = false;
}

void term_damage_all(Term* t)
{
	t->sixels_dirty = true;
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
	if (RUNE(t, x, y).width == 0 && x > 0)
		RUNE(t, x - 1, y).dmg = true;
	if (RUNE(t, x, y).width == 2 && x + 1 < t->cols)
		RUNE(t, x + 1, y).dmg = true;
	int first = RUNE(t, x, y).width == 0 && x > 0 ? x - 1 : x;
	int width = RUNE(t, first, y).width == 2 ? 2 : 1;
	for (size_t i = 0; i < t->sixels.count; i++) {
		if (sixel_overlaps(t, &t->sixels.views[i], first, y, width, 1)) {
			t->sixels_dirty = true;
			break;
		}
	}
}

void term_free(Term* t)
{
	sixel_clear(&t->sixels);
	sixel_clear(&t->alt_sixels);
	free(t->runes);
	free(t->alt);
	free(t->dcs_data);
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
		int width = uleutf_codepointvisualwidth(cp);
		if (width <= 0)
			break;
		if (width == 2 && t->cols < 2)
			width = 1;

		if (t->wrapnext || c->x + width > t->cols) {
			term_putc(t, c, '\n');
			t->wrapnext = false;
		}

		sixel_erase(t, c->x, c->y, width, 1);
		rune_prepare(t, c->x, c->y);
		if (width == 2)
			rune_prepare(t, c->x + 1, c->y);
		Rune* r = &RUNE(t, c->x, c->y);
		if (r->cp != cp || r->fg != t->fg || r->bg != t->bg ||
		    r->attr != t->attr || r->width != width) {
			r->cp = cp;
			r->fg = t->fg;
			r->bg = t->bg;
			r->attr = t->attr;
			r->width = width;
			r->dmg = true;
		}
		if (width == 2) {
			Rune* tail = &RUNE(t, c->x + 1, c->y);
			tail->cp = ' ';
			tail->fg = t->fg;
			tail->bg = t->bg;
			tail->attr = t->attr;
			tail->width = 0;
			tail->dmg = true;
		}

		if (c->x + width >= t->cols)
			t->wrapnext = true;
		else
			c->x += width;

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
		sixel_clear(&t->sixels);
		t->sixels = t->alt_sixels;
		t->alt_sixels = (SixelLayer){ 0 };
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
	Rune* nr = malloc((size_t)cols * rows * sizeof(*nr));
	if (!nr)
		return -1;
	Rune blank = { .cp = ' ', .fg = t->fg, .bg = t->bg,
	               .attr = t->attr, .width = 1, .dmg = true };
	for (int i = 0; i < cols * rows; i++)
		nr[i] = blank;

	if (or) {
		int copyrows = orows < rows ? orows : rows;
		int copycols = ocols < cols ? ocols : cols;

		for (int y = 0; y < copyrows; y++)
			memcpy( &nr[y * cols], &or[y * ocols], sizeof(Rune) * copycols);
	}

	for (int i = 0; i < cols * rows; i++)
		nr[i].dmg = true;

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
	sixel_scroll(t, 0, rows - 1, 0);
	t->sixels_dirty = true;

	return 0;
}

void term_scroll(Term* t)
{
	scroll_region(t, t->scroll_top, t->scroll_bot, -1);
}

bool term_write(Term* t, Caret* c, const char* s, size_t n)
{
	bool damaged = false;

	for (size_t i = 0; i < n; i++) {
		unsigned char ch = (unsigned char)s[i];
		Caret old = *c;

		switch (t->state) {
		case TSTATE_NORMAL:
			if (t->utf8_need) {
				if ((ch & 0xc0) == 0x80) {
					t->utf8_buf[t->utf8_len++] = ch;
					if (t->utf8_len == t->utf8_need) {
						ule_i32 cp;
						t->utf8_buf[t->utf8_len] = '\0';
						if (uleutf_valid(t->utf8_buf) == NULL) {
							uleutf_codepoint(t->utf8_buf, &cp);
							term_putc(t, c, (uint32_t)cp);
						}
						else
							term_putc(t, c, 0xfffd);
						t->utf8_len = t->utf8_need = 0;
						damaged = true;
					}
					break;
				}
				t->utf8_len = t->utf8_need = 0;
				term_putc(t, c, 0xfffd);
			}
			if (ch == '\x1B')
				t->state = TSTATE_ESC;
			else if (ch == '\x0E')
				t->charset = 1;
			else if (ch == '\x0F')
				t->charset = 0;
			else if (ch >= 0x80) {
				if (ch >= 0xc2 && ch <= 0xdf)
					t->utf8_need = 2;
				else if (ch >= 0xe0 && ch <= 0xef)
					t->utf8_need = 3;
				else if (ch >= 0xf0 && ch <= 0xf4)
					t->utf8_need = 4;
				else
					term_putc(t, c, 0xfffd);
				if (t->utf8_need) {
					t->utf8_buf[0] = ch;
					t->utf8_len = 1;
				}
			}
			else {
				if (ch == '\n' && c->y == t->scroll_bot)
					damaged = true;
				term_putc(t, c, t->acs[t->charset] ? acs_map(ch) : ch);
				if (ch >= ' ' && ch < 0x7f)
					damaged = true;
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
			else if (ch == 'P') {
				t->dcs_len = 0;
				t->dcs_sixel = true;
				t->state = TSTATE_DCS;
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
				if (c->y == t->scroll_top) {
					scroll_region(t, t->scroll_top, t->scroll_bot, 1);
					damaged = true;
				}
				else if (c->y > 0)
					c->y--;
				t->state = TSTATE_NORMAL;
			}
			else if (ch == 'D' || ch == 'E') {
				t->wrapnext = false;
				if (ch == 'E')
					c->x = 0;
				if (c->y == t->scroll_bot) {
					scroll_region(t, t->scroll_top, t->scroll_bot, -1);
					damaged = true;
				}
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
			else if (ch >= 0x20 && ch <= 0x2f) {
				/* CSI intermediate byte */
			}
			else if (ch >= 0x40 && ch <= 0x7e) {
				csi_dispatch(t, c, ch);
				damaged = true;
				t->state = TSTATE_NORMAL;
			}
			break;

		case TSTATE_DCS:
			if (ch == '\x1b')
				t->state = TSTATE_DCS_ESC;
			else if (ch == 0x18 || ch == 0x1a || ch == 0x9c) {
				t->dcs_len = 0;
				t->state = TSTATE_NORMAL;
			}
			else if ((ch >= '0' && ch <= '9') || ch == ';') {
				/* DCS parameters */
			}
			else if (ch >= 0x40 && ch <= 0x7e) {
				t->dcs_sixel = t->dcs_sixel && ch == 'q';
				t->state = TSTATE_DCS_DATA;
			}
			else
				t->dcs_sixel = false;
			break;

		case TSTATE_DCS_DATA:
			if (ch == '\x1b')
				t->state = TSTATE_DCS_ESC;
			else if (ch == 0x9c) {
				damaged |= dcs_finish(t, c);
				t->state = TSTATE_NORMAL;
			}
			else if (ch == 0x18 || ch == 0x1a) {
				t->dcs_len = 0;
				t->state = TSTATE_NORMAL;
			}
			else
				dcs_append(t, ch);
			break;

		case TSTATE_DCS_ESC:
			if (ch == '\\') {
				damaged |= dcs_finish(t, c);
				t->state = TSTATE_NORMAL;
			}
			else {
				t->dcs_len = 0;
				t->dcs_sixel = false;
				t->state = TSTATE_ESC;
				i--;
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

	return damaged;
}

