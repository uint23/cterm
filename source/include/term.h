#ifndef TERM_H
#define TERM_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/* resolve 1D rune from 2D input */ 
#define RUNE(t, x, y) ((t)->runes[(y) * (t)->cols + (x)])
#define CSI_PARAMS_MAX (16)
#define TERM_DEFAULT_FG 0xffffffffu
#define TERM_DEFAULT_BG 0xff222222u
#define TERM_ATTR_REVERSE (1 << 0)

enum TState {
	TSTATE_NORMAL,
	TSTATE_ESC,
	TSTATE_CSI,
	TSTATE_OSC,
	TSTATE_OSC_ESC,
	TSTATE_CHARSET,
	TSTATE_CHARSET_SKIP,
};

typedef struct {
	uint32_t       cp;  /* codepoint */
	uint32_t       fg;
	uint32_t       bg;
	uint8_t        attr;
	uint8_t        width;
	bool           dmg; /* rune damaged */
} Rune;

typedef struct {
	int            x;
	int            y;
} Caret;

typedef struct {
	Rune*          runes;
	Rune*          alt;
	Caret          saved;
	uint32_t       fg;
	uint32_t       bg;
	uint8_t        attr;

	int            cols;
	int            rows;
	int            scroll_top;
	int            scroll_bot;

	int            ptyfd;
	pid_t          ptypid;

	enum TState    state;
	bool           acs[2];
	int            charset;
	int            charset_target;
	bool           wrapnext;
	bool           cursor_visible;
	int            csi_params[CSI_PARAMS_MAX];
	int            csi_idx;
	char           utf8[4];
	size_t         utf8_len;
} Term;

/**
 * @brief clear screen by filling runes with ' ' and reseting
 *        to default terminal foreground and background
 *
 * @param t terminal instance to scroll
 * @param c cursor instance to position
 */
void term_clear(Term* t, Caret* c);

/**
 * @brief damage all the runes on terminal
 */
void term_damage_all(Term* t);

/** @brief damage a single cell on terminal
 */
void term_damage_rune(Term* t, int x, int y);

/**
 * @brief handle placing special and normal characters
 *
 * @param t terminal instance
 * @param c cursor instance
 * @param cp codepoint to place
 */
void term_putc(Term* t, Caret* c, uint32_t cp);

/** TODO
 */
int term_resize(Term* t, Caret* c, int cols, int rows);

/**
 * @brief scroll viewport up by one
 *
 * @param t terminal instance to scroll
 */
void term_scroll(Term* t);

/**
 * @brief handle placing special and normal characters
 *
 * @param t terminal instance
 * @param c cursor instance
 * @param s string buffer to write
 * @param n length of string buffer
 * 
 * @return whether it has been damaged
 */
bool term_write(Term* t, Caret* c, const char* s, size_t n);

#endif /* TERM_H */

