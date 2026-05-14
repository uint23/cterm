#define RGFW_IMPLEMENTATION
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#ifdef __linux__
#include <pty.h>
#else
#include <util.h>
#endif
#include <unistd.h>

#include <RGFW.h>
#include <schrift.h>

#include "draw.h"
#include "font.h"
#include "term.h"
#include "utils.h"

#define START_COLS   80
#define START_ROWS   24

static void cleanup(void);
static void handle_key(RGFW_event ev);
static void init(void);
static bool ptyread(void);
static void ptywrite(const char* s, size_t n);
static void resize_pty(void);
static int resize_surface(int w, int h);
static void resize_terminal(int w, int h);
static void run(void);

static Fontface font;
static Caret car;
static RGFW_window* win = NULL;
static RGFW_surface* surf = NULL;
static uint32_t* pixels = NULL;
static int winw;
static int winh;

static Term term = {
	.runes = NULL,
	.cols = 0,
	.rows = 0,
	.ptyfd = -1,
	.ptypid = -1,
	.fg = TERM_DEFAULT_FG,
	.bg = TERM_DEFAULT_BG,
	.cursor_visible = true,
};

/**
 * @brief release resources, close window (TODO)
 */
static void cleanup(void)
{
	free(term.runes);
	free(term.alt);
	font_free(&font);

	if (surf)
		RGFW_surface_free(surf);
	if (pixels)
		free(pixels);
	if (win)
		RGFW_window_close(win);
}

/**
 * @brief dispatch correct key to pty
 *
 * @param c character check dispatch
 */
static void handle_key(RGFW_event ev)
{
	/* regular character-
	 * dont have to handle e.g. shift cases:
	 * SHIFT+1 or `!`
	 */
	if (ev.type == RGFW_keyChar) {
		uint32_t cp = ev.keyChar.value;
		if (cp >= ' ' && cp <= '~') {
			char cpch = (char)cp;
			ptywrite(&cpch, 1);
		}
	}

	/* special character */
	if (ev.type == RGFW_keyPressed) {
		RGFW_key key = ev.key.value;
		if ((ev.key.mod & RGFW_modControl) &&
		    key >= RGFW_keyA && key <= RGFW_keyZ) {
			char c = (char)(key - RGFW_keyA + 1);
			ptywrite(&c, 1);
			return;
		}

		switch(key) {
		case RGFW_keyReturn:
		case RGFW_keyPadReturn: ptywrite("\r", 1); break;
		case RGFW_keyBackSpace: ptywrite("\x7F", 1); break;
		case RGFW_keyTab:       ptywrite("\t",  1); break;
		case RGFW_keyEscape:    ptywrite("\x1B", 1); break;
		case RGFW_keyUp:        ptywrite("\x1B[A", 3); break;
		case RGFW_keyDown:      ptywrite("\x1B[B", 3); break;
		case RGFW_keyRight:     ptywrite("\x1B[C", 3); break;
		case RGFW_keyLeft:      ptywrite("\x1B[D", 3); break;
		case RGFW_keyHome:      ptywrite("\x1B[H", 3); break;
		case RGFW_keyEnd:       ptywrite("\x1B[F", 3); break;
		case RGFW_keyInsert:    ptywrite("\x1B[2~", 4); break;
		case RGFW_keyDelete:    ptywrite("\x1B[3~", 4); break;
		case RGFW_keyPageUp:    ptywrite("\x1B[5~", 4); break;
		case RGFW_keyPageDown:  ptywrite("\x1B[6~", 4); break;
		default: break;
		}
	}
}

/**
 * @brief initialise: window (TODO)
 */
static void init(void)
{
	/* font */
	/* TODO: paths */
	if (font_load(&font, "./Xanh.ttf", 24.0) < 0)
		die(1, "failed to load font");
	font.aa = true;

	winw = START_COLS*font.cellw;
	winh = START_ROWS*font.cellh;

	if (term_resize(&term, &car, START_COLS, START_ROWS) < 0)
		die(1, "failed to resize terminal");

	/* window */
	if (!(win = RGFW_createWindow("cterm", 0, 0, winw, winh, 0)))
		die(1, "failed to create window");

	pixels = calloc(winw*winh, sizeof(*pixels));
	if (!pixels)
		die(1, "failed to alloc pixel buffer");

	surf = RGFW_window_createSurface(
		win, (u8*)pixels, winw, winh, RGFW_formatRGBA8
	);
	if (!surf)
		die(1, "failed to create window surface");

	/* pty */
	struct winsize ws = {
		.ws_col = term.cols,
		.ws_row = term.rows,
		.ws_xpixel = winw,
		.ws_ypixel = winh
	};
	term.ptypid = forkpty(&term.ptyfd, NULL, NULL, &ws);
	if (term.ptypid < 0)
		die(EXIT_FAILURE, "failed to fork pty");
	if (term.ptypid == 0) {
		/* TODO: change later */
		setenv("TERM", "xterm-256color", 1);
		execlp("/bin/sh", "sh", "-i", NULL);
		_exit(127);
	}

	int flags = fcntl(term.ptyfd, F_GETFL, 0);
	if (flags >= 0) /* append to existing flags */
		fcntl(term.ptyfd, F_SETFL, flags|O_NONBLOCK);

	term_clear(&term, &car);
}

/**
 * @brief read data from master pty
 */
static bool ptyread(void)
{
	char buf[4096];
	bool damaged = false;

	for (;;) {
		ssize_t n = read(term.ptyfd, buf, sizeof(buf));
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			break;
		if (n > 0) {
			if (term_write(&term, &car, buf, n))
				damaged = true;
			continue;
		}

		/* EOF or other error */
		break;
	}

	return damaged;
}

/**
 * @brief read data to master pty
 *
 * @param s string buffer to write to pty
 * @param n number of characters in buffer
 */
static void ptywrite(const char* s, size_t n)
{
	while (n > 0) {
		ssize_t ret = write(term.ptyfd, s, n);
		if (ret < 0 && (errno == EINTR))
			continue;
		if (ret > 0) {
			s+= ret;
			n -= ret;
			continue;
		}

		break;
	}
}

/** TODO
 */
static void resize_pty(void)
{
	struct winsize ws = {
		.ws_col = term.cols,
		.ws_row = term.rows,
		.ws_xpixel = winw,
		.ws_ypixel = winh,
	};

	if (term.ptyfd >= 0)
		ioctl(term.ptyfd, TIOCSWINSZ, &ws);
}

/** TODO
 */
static int resize_surface(int w, int h)
{
	uint32_t* npixels;
	RGFW_surface* nsurf;

	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;

	npixels = calloc(w*h, sizeof(*npixels));
	if (!npixels)
		return -1;

	nsurf = RGFW_window_createSurface(
		win, (u8*)npixels, w, h, RGFW_formatRGBA8
	);
	if (!nsurf) {
		free(npixels);
		return -1;
	}

	if (surf)
		RGFW_surface_free(surf);
	free(pixels);

	pixels = npixels;
	surf = nsurf;
	winw = w;
	winh = h;

	return 0;
}

/** TODO
 */
static void resize_terminal(int w, int h)
{
	int cols = w / font.cellw;
	int rows = h / font.cellh;

	if (cols < 1)
		cols = 1;
	if (rows < 1)
		rows = 1;

	if (cols == term.cols && rows == term.rows)
		return;

	if (term_resize(&term, &car, cols, rows) < 0)
		die(1, "failed to resize terminal");

	resize_pty();
}

/**
 * @brief program event loop (TODO)
 */
static void run(void)
{
	RGFW_event ev;
	Caret ocar = { -1, -1 };
	bool dirty = true;
	bool redraw_all = true;

	while (!RGFW_window_shouldClose(win)) {
		while (RGFW_window_checkEvent(win, &ev)) {
			if (ev.type == RGFW_windowResized) {
				if (resize_surface(ev.update.w, ev.update.h) < 0)
					die(EXIT_FAILURE, "failed to resize surface");
				resize_terminal(ev.update.w, ev.update.h);
				term_damage_all(&term);
				dirty = true;
				continue;
			}

			if (ev.type == RGFW_windowRefresh ||
			    ev.type == RGFW_windowFocusIn ||
			    ev.type == RGFW_windowRestored) {
				term_damage_all(&term);
				dirty = true;
				redraw_all = true;
				continue;
			}

			handle_key(ev);
		}

		if (ptyread())
			dirty = true;

		if (ocar.x != car.x || ocar.y != car.y) {
			term_damage_rune(&term, ocar.x, ocar.y);
			term_damage_rune(&term, car.x, car.y);
			dirty = true;
		}

		if (!dirty)
			continue;

		if (redraw_all) {
			draw_clear(pixels, winw, winh, rgba(0, 0, 0, 255));
		}

		for (int y = 0; y < term.rows; y++) {
			for (int x = 0; x < term.cols; x++) {
				Rune* r = &RUNE(&term, x, y);
				bool cursor = term.cursor_visible &&
				              x == car.x && y == car.y;
				bool reverse = (r->attr & TERM_ATTR_REVERSE) != 0;
				uint32_t fg = (cursor != reverse) ? r->bg : r->fg;
				uint32_t bg = (cursor != reverse) ? r->fg : r->bg;

				if (!r->dmg)
					continue;

				draw_rune(
					pixels, winw, winh, x, y,
					font.cellw, font.cellh, bg
				);

				if (r->cp != ' ') {
					draw_codepoint(
						&font, pixels, winw, winh,
						x * font.cellw,
						y * font.cellh + (int)font.asc,
						r->cp,
						fg
					);
				}

				r->dmg = false;
			}
		}

		RGFW_window_blitSurface(win, surf);

		ocar = car;
		dirty = false;
		redraw_all = false;
	}
}

int main(int argc, char* argv[])
{
	/* TODO: pledges */
	init();
	run();
	cleanup();

	return EXIT_SUCCESS;
	(void) argc, (void) argv;
}

