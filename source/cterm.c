#define RGFW_IMPLEMENTATION
#include <stdlib.h>

#include <RGFW.h>
#include <grapheme.h>
#include <schrift.h>

#include "util.h"

RGFW_window* win = NULL;

void init(void);
void run(void);

/**
 * @brief initialise: window (TODO)
 */
void init(void)
{
	if (!(win = RGFW_createWindow("cterm", 0, 0, 800, 600, 0)))
		die(1, "failed to create window");
}

/**
 * @brief program event loop (TODO)
 */
void run(void)
{
	RGFW_event ev;
	while (!RGFW_window_shouldClose(win)) {
		while (RGFW_window_checkEvent(win, &ev))
			;
	}

	RGFW_window_close(win);
}

int main(int argc, char* argv[])
{
	/* TODO: pledges */
	init();
	run();

	return EXIT_SUCCESS;
	(void) argc, (void) argv;
}

