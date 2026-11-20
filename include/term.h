#ifndef TERM_H
#define TERM_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "sixel.h"

/* resolve 1D rune from 2D input */ 
#define RUNE(t, x, y) ((t)->runes[(y) * (t)->cols + (x)])
#define CSI_PARAMS_MAX (16)
#define TERM_DEFAULT_FG 0xffffffffu
#define TERM_DEFAULT_BG 0xff222222u
#define TERM_ATTR_REVERSE (1 << 0)

typedef enum {
	TSTATE_NORMAL,
	TSTATE_ESC,
	TSTATE_CSI,
	TSTATE_DCS,
	TSTATE_DCS_DATA,
	TSTATE_DCS_ESC,
	TSTATE_OSC,
	TSTATE_OSC_ESC,
	TSTATE_CHARSET,
	TSTATE_CHARSET_SKIP,
} TState;

typedef struct {
	uint32_t cp;  /* codepoint */
	uint32_t fg;
	uint32_t bg;
	uint8_t  attr;
	uint8_t  width;
	bool     dmg; /* rune damaged */
} Rune;

typedef struct {
	int x;
	int y;
} Caret;

typedef struct {
	Rune*    runes;
	Rune*    alt;
	Caret    saved;
	uint32_t fg;
	uint32_t bg;
	uint8_t  attr;

	int cols;
	int rows;
	int scroll_top;
	int scroll_bot;

	int   ptyfd;
	pid_t ptypid;

	TState  state;
	char    utf8_buf[5];
	uint8_t utf8_len;
	uint8_t utf8_need;
	bool    acs[2];
	int     charset;
	int     charset_target;
	bool    wrapnext;
	bool    cursor_visible;
	bool    bracketed_paste;
	int     csi_params[CSI_PARAMS_MAX];
	int     csi_idx;
	char*   dcs_data;
	size_t  dcs_len;
	size_t  dcs_cap;
	bool    dcs_sixel;
	Sixel   sixel;
	Caret   sixel_pos;
} Term;

/* clear screen by filling runes with ' ' and reseting
   to default terminal foreground and background */
void term_clear(Term* t, Caret* c);

/* damage all the runes on terminal */
void term_damage_all(Term* t);

/* damage a single cell on terminal */
void term_damage_rune(Term* t, int x, int y);

/* put a character on terminal, handle placing special
   and normal characters */
void term_putc(Term* t, Caret* c, uint32_t cp);

/* destroy old terminal and create create new terminal
   with specified dimensions */
int term_resize(Term* t, Caret* c, int cols, int rows);

/* scroll viewport up by one */
void term_scroll(Term* t);

/* processes terminal input, parse escape sequences, update
   caret, screen state, and damage status. */
bool term_write(Term* t, Caret* c, const char* s, size_t n);

#endif /* TERM_H */

