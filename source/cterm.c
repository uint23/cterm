#define RGFW_IMPLEMENTATION
#include <stdlib.h>

#include <RGFW.h>
#include <grapheme.h>
#include <schrift.h>

int main(int argc, char* argv[])
{
	RGFW_window* win = RGFW_createWindow("cterm", 0, 0, 800, 600, 0);
	RGFW_event ev;

	while (!RGFW_window_shouldClose(win)) {
		while (RGFW_window_checkEvent(win, &ev))
			;
	}


	RGFW_window_close(win);

	return EXIT_SUCCESS;
	(void) argc, (void) argv;
}

