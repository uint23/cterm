#define RGFW_IMPLEMENTATION
#include <stdint.h>
#include <stdlib.h>

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
	.ptyfd = -1, /* TODO: pty */
	.fg = 0xffffffff,
	.bg = 0xff222222,
};

static void cleanup(void);
static void init(void);
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
 * @brief initialise: window (TODO)
 */
static void init(void)
{
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

	/* tmp */
	term_clear(&term, &car);
	const char* s =
		"cterm test\n"
		"terminal buffer!";
	for (const char* p = s; *p; p++)
		term_putc(&term, &car, (unsigned char)*p);
}

/**
 * @brief program event loop (TODO)
 */
static void run(void)
{
	RGFW_event ev;

	while (!RGFW_window_shouldClose(win)) {
		while (RGFW_window_checkEvent(win, &ev))
			;

		draw_clear(pixels, WIDTH, HEIGHT, rgba(0, 0, 0, 255));

		for (int y = 0; y < term.rows; y++) {
			for (int x = 0; x < term.cols; x++) {
				Rune* rune  = &RUNE(&term, x, y);

				draw_rune(pixels, WIDTH, HEIGHT,
				          x, y,
				          font.cellw, font.cellh,
				          rune->bg);

				if (rune->cp != ' ') {
					draw_codepoint(&font, pixels, WIDTH, HEIGHT,
					               x * font.cellw,
					               y * font.cellh + (int)font.asc,
					               rune->cp,
					               rune->fg);
				}
			}
		}

		draw_caret(pixels, WIDTH, HEIGHT,
		            car.x, car.y,
		            font.cellw, font.cellh,
		            rgba(255, 255, 255, 255));

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

