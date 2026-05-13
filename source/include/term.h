#ifndef TERM_H
#define TERM_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/* resolve 1D rune from 2D input */ 
#define RUNE(t, x, y) ((t)->runes[(y) * (t)->cols + (x)])

typedef struct {
	uint32_t       cp;  /* codepoint */
	uint32_t       fg;
	uint32_t       bg;
	bool           dmg; /* rune damaged */
} Rune;

typedef struct {
	int            x;
	int            y;
} Caret;

typedef struct {
	Rune*          runes;
	uint32_t       fg;
	uint32_t       bg;
	int            cols;
	int            rows;
	int            ptyfd;
	pid_t          ptypid;
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
 * @brief handle placing special and normal characters
 *
 * @param t terminal instance
 * @param c cursor instance
 * @param cp codepoint to place
 */
void term_putc(Term* t, Caret* c, uint32_t cp);

/**
 * @brief scroll viewport up by one
 *
 * @param t terminal instance to scroll
 */
void term_scroll(Term* t);

#endif /* TERM_H */

