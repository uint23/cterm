#define RGFW_IMPLEMENTATION
#include <stdint.h>
#include <stdlib.h>

#include <RGFW.h>
#include <grapheme.h>
#include <schrift.h>

#include "draw.h"
#include "util.h"

#define WIDTH  800
#define HEIGHT 600
#define CELLW  9
#define CELLH  16

RGFW_window* win = NULL;
RGFW_surface* surf = NULL;
uint32_t* pixels = NULL;

static void init(void);
static void run(void);

/**
 * @brief initialise: window (TODO)
 */
static void init(void)
{
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

		draw_clear(pixels, WIDTH, HEIGHT, rgba(255, 0, 0, 255)); /* TODO: opacity */

		draw_cell(pixels, WIDTH, HEIGHT, 0, 0, CELLW, CELLH, rgba(255, 0, 0, 255));
		draw_cell(pixels, WIDTH, HEIGHT, 1, 0, CELLW, CELLH, rgba(0, 255, 0, 255));
		draw_cell(pixels, WIDTH, HEIGHT, 2, 0, CELLW, CELLH, rgba(0, 0, 255, 255));
		draw_cursor(pixels, WIDTH, HEIGHT, 3, 0, CELLW, CELLH, rgba(255, 255, 255, 255));

		RGFW_window_blitSurface(win, surf);
	}
}

int main(int argc, char* argv[])
{
	/* TODO: pledges */
	init();
	run();

	return EXIT_SUCCESS;
	(void) argc, (void) argv;
}

