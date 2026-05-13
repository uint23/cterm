#define RGFW_IMPLEMENTATION
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <pty.h>
#else
#include <util.h>
#endif
#include <unistd.h>

#include <RGFW.h>
#include <grapheme.h>
#include <schrift.h>

#include "draw.h"
#include "font.h"
#include "term.h"
#include "utils.h"

#define WIDTH  800
#define HEIGHT 600

#define COLS   80
#define ROWS   24

static Fontface font;
static Caret car;
static RGFW_window* win = NULL;
static RGFW_surface* surf = NULL;
static uint32_t* pixels = NULL;

static Rune runes[COLS*ROWS]; /* TODO: dynamic */
static Term term = {
	.runes = runes,
	.cols = COLS,
	.rows = ROWS,
	.ptyfd = -1,
	.ptypid = -1,
	.fg = 0xffffffff,
	.bg = 0xff222222,
};

static void cleanup(void);
static void handle_key(RGFW_event ev);
static void init(void);
static void ptyread(void);
static void ptywrite(const char* s, size_t n);
static void run(void);

/**
 * @brief release resources, close window (TODO)
 */
static void cleanup(void)
{
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
		char c = ev.key.value;
		switch(c) {
		case RGFW_keyReturn:    ptywrite("\r", 1); break;

		case RGFW_keyDelete:
		case RGFW_keyBackSpace: ptywrite("\x7F", 1); break;

		case RGFW_keyTab:       ptywrite("\t", 1); break;
		case RGFW_keyEscape:    ptywrite("\x1B", 1); break;
		default: break;
		}
	}
}

/**
 * @brief initialise: window (TODO)
 */
static void init(void)
{
	/* pty */
	struct winsize ws = {
		.ws_col = COLS,
		.ws_row = ROWS,
		.ws_xpixel = WIDTH,
		.ws_ypixel = HEIGHT
	};
	term.ptypid = forkpty(&term.ptyfd, NULL, NULL, &ws);
	if (term.ptypid < 0)
		die(EXIT_FAILURE, "failed to fork pty");
	if (term.ptypid == 0) {
		/* TODO: change later */
		setenv("TERM", "dumb", 1);
		execlp("/bin/sh", "sh", "-i", NULL);
		_exit(127);
	}

	int flags = fcntl(term.ptyfd, F_GETFL, 0);
	if (flags >= 0) /* append to existing flags */
		fcntl(term.ptyfd, F_SETFL, flags|O_NONBLOCK);

	/* window */
	if (!(win = RGFW_createWindow("cterm", 0, 0, 800, 600, 0)))
		die(1, "failed to create window");

	pixels = calloc(WIDTH*HEIGHT, sizeof(*pixels));
	if (!pixels)
		die(1, "failed to alloc pixel buffer");

	surf = RGFW_window_createSurface(
		win, (u8*)pixels, WIDTH, HEIGHT, RGFW_formatRGBA8
	);
	if (!surf)
		die(1, "failed to create window surface");

	/* font */
	/* TODO: paths */
	if (font_load(&font, "./Xanh.ttf", 24.0) < 0)
		die(1, "failed to load font");
	font.aa = false;
	term_clear(&term, &car);
}

/**
 * @brief read data from master pty
 */
static void ptyread(void)
{
	char buf[4096];

	for (;;) {
		ssize_t n = read(term.ptyfd, buf, sizeof(buf));
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			break;
		if (n > 0) {
			for (ssize_t i = 0; i < n; i++)
				term_putc(&term, &car, (unsigned char)buf[i]);
			continue;
		}

		/* EOF or other error */
		break;
	}
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

/**
 * @brief program event loop (TODO)
 */
static void run(void)
{
	RGFW_event ev;

	while (!RGFW_window_shouldClose(win)) {
		while (RGFW_window_checkEvent(win, &ev))
			handle_key(ev);

		ptyread();

		/* drawing */
		draw_clear(pixels, WIDTH, HEIGHT, rgba(0, 0, 0, 255));
		for (int y = 0; y < term.rows; y++) {
			for (int x = 0; x < term.cols; x++) {
				Rune* rune  = &RUNE(&term, x, y);

				draw_rune(
					pixels, WIDTH, HEIGHT, x, y,
					font.cellw, font.cellh, rune->bg
				);

				if (rune->cp != ' ') {
					draw_codepoint(
						&font, pixels, WIDTH, HEIGHT,
						x * font.cellw,
						y * font.cellh + (int)font.asc,
						rune->cp, rune->fg
					);
				}
			}
		}

		draw_caret(
			pixels, WIDTH, HEIGHT,
			car.x, car.y,
			font.cellw, font.cellh,
			rgba(255, 255, 255, 255)
		);

		RGFW_window_blitSurface(win, surf);
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

