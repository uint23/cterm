#define RGFW_IMPLEMENTATION
#include <stdint.h>
#include <stdlib.h>

#include <RGFW.h>
#include <grapheme.h>
#include <schrift.h>

#include "draw.h"
#include "font.h"
#include "util.h"

#define WIDTH  800
#define HEIGHT 600
#define CELLW  18
#define CELLH  32

Font font;
RGFW_window* win = NULL;
RGFW_surface* surf = NULL;
uint32_t* pixels = NULL;

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
	if (font_load(&font, "/System/Library/Fonts/Supplemental/Andale Mono.ttf", 24.0) < 0)
		die(1, "failed to load font");
	font.aa = false;
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

		draw_clear(pixels, WIDTH, HEIGHT, rgba(0, 0, 0, 255)); /* TODO: opacity */

		draw_cell(pixels, WIDTH, HEIGHT, 0, 0, CELLW, CELLH, rgba(255, 0, 0, 255));
		draw_cell(pixels, WIDTH, HEIGHT, 1, 0, CELLW, CELLH, rgba(0, 255, 0, 255));
		draw_cell(pixels, WIDTH, HEIGHT, 2, 0, CELLW, CELLH, rgba(0, 0, 255, 255));
		draw_cursor(pixels, WIDTH, HEIGHT, 3, 0, CELLW, CELLH, rgba(255, 255, 255, 255));

		/* text */
		font_draw_codepoint(&font, pixels, WIDTH, HEIGHT,
				0 * CELLW, (int)font.asc, 'r', rgba(0, 0, 0, 255));

		font_draw_codepoint(&font, pixels, WIDTH, HEIGHT,
				1 * CELLW, (int)font.asc, 'g', rgba(0, 0, 0, 255));

		font_draw_codepoint(&font, pixels, WIDTH, HEIGHT,
				2 * CELLW, (int)font.asc, 'b', rgba(0, 0, 0, 255));

		font_draw_codepoint(&font, pixels, WIDTH, HEIGHT,
				3 * CELLW, (int)font.asc, 'c', rgba(0, 0, 0, 255));

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

