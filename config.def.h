#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static const char font_path[] = "monospace.bdf";

static const int   win_width = 80;  /* in columns */
static const int   win_height = 24; /* in rows */
static const char  win_title[] = "cterm";
static const int   win_fps = 60;

static const int   pad_x = 8;
static const int   pad_y = 8;

static const char* const shell = NULL; /* NULL uses $SHELL */
static const char term_name[] = "vt100";

#define CTRL_DOWN  (win->key_syms[MAUS_KEY_CONTROL_L] || win->key_syms[MAUS_KEY_CONTROL_R])
#define SHIFT_DOWN (win->key_syms[MAUS_KEY_SHIFT_L] || win->key_syms[MAUS_KEY_SHIFT_R])

#define BIND_COPY  (CTRL_DOWN && SHIFT_DOWN && \
                   (ev->key.key == MAUS_KEY_C || ev->key.key == MAUS_KEY_C_UP))

#define BIND_PASTE (CTRL_DOWN && SHIFT_DOWN && \
                   (ev->key.key == MAUS_KEY_V || ev->key.key == MAUS_KEY_V_UP))

#define BIND_RELOAD_FONT (win->key_syms[MAUS_KEY_ALT_R] && \
                         (ev->key.key == MAUS_KEY_R ||     \
                         ev->key.key == MAUS_KEY_R_UP))

/* AARRGGBB */
static const uint32_t default_fg = 0xffffffff;
static const uint32_t default_bg = 0xff000000;
static const uint32_t cursor_fg  = 0xff000000;
static const uint32_t cursor_bg  = 0xffffffff;

static const uint32_t color_table[16] = {
	0xff000000, /* black */
	0xffcc0000, /* red */
	0xff4e9a06, /* green */
	0xffc4a000, /* yellow */
	0xff3465a4, /* blue */
	0xff75507b, /* magenta */
	0xff06989a, /* cyan */
	0xffd3d7cf, /* white */

	0xff555753, /* bright black */
	0xffef2929, /* bright red */
	0xff8ae234, /* bright green */
	0xfffce94f, /* bright yellow */
	0xff729fcf, /* bright blue */
	0xffad7fa8, /* bright magenta */
	0xff34e2e2, /* bright cyan */
	0xffffffff, /* bright white */
};

#endif /* CONFIG_H */

