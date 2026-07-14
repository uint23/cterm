#include "maus_input.h"
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
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

#include <maus.h>

#include "../config.h"
#include "draw.h"
#include "font.h"
#include "term.h"
#include "utils.h"

static void cleanup(void);
static bool handle_key(const MausEvent* ev);
static void init(void);
static bool ptyread(void);
static void ptywrite(const char* s, size_t n);
static bool reload_font(void);
static void resize_pty(void);
static int resize_window(int w, int h);
static void resize_terminal(int w, int h);
static void run(void);
static void wait_events(void);

static Fontface font;
static Caret car;
static Maus* win = NULL;
static int winw;
static int winh;

static Term term = {
	.runes = NULL,
	.cols = 0,
	.rows = 0,
	.ptyfd = -1,
	.ptypid = -1,
	.fg = default_fg,
	.bg = default_bg,
	.cursor_visible = true,
};

/* clean up resources */
static void cleanup(void)
{
	free(term.runes);
	free(term.alt);
	font_free(&font);

	if (win) {
		maus_close(win);
		free(win);
	}
}

/* dispatch correct keycode handling non ascii characters */
static bool handle_key(const MausEvent* ev)
{
	char text;

	if (ev->type != MAUS_EV_KEY || !ev->key.pressed)
		return false;

	if (win->key_syms[MAUS_KEY_ALT_R] &&
	   (ev->key.key == MAUS_KEY_R ||
	    ev->key.key == MAUS_KEY_R_UP))
		return reload_font();

	switch(ev->key.key) {
	case MAUS_KEY_ENTER:
	case MAUS_KEY_KP_ENTER: ptywrite("\r", 1); break;
	case MAUS_KEY_BACKSPACE: ptywrite("\x7F", 1); break;
	case MAUS_KEY_TAB:       ptywrite("\t",  1); break;
	case MAUS_KEY_ESCAPE:    ptywrite("\x1B", 1); break;
	case MAUS_KEY_UP:        ptywrite("\x1B[A", 3); break;
	case MAUS_KEY_DOWN:      ptywrite("\x1B[B", 3); break;
	case MAUS_KEY_RIGHT:     ptywrite("\x1B[C", 3); break;
	case MAUS_KEY_LEFT:      ptywrite("\x1B[D", 3); break;
	case MAUS_KEY_HOME:      ptywrite("\x1B[H", 3); break;
	case MAUS_KEY_END:       ptywrite("\x1B[F", 3); break;
	case MAUS_KEY_INSERT:    ptywrite("\x1B[2~", 4); break;
	case MAUS_KEY_DELETE:    ptywrite("\x1B[3~", 4); break;
	case MAUS_KEY_PAGE_UP:   ptywrite("\x1B[5~", 4); break;
	case MAUS_KEY_PAGE_DOWN: ptywrite("\x1B[6~", 4); break;
	default:
		text = ev->key.text;
		if (text != '\0')
			ptywrite(&text, 1);
		break;
	}

	return false;
}

/* initialise font, window, pty */
static void init(void)
{
	setlocale(LC_CTYPE, "");

	/* font */
	if (font_load(&font, font_path) < 0)
		die(1, "failed to load font");

	winw = win_width*font.cellw;
	winh = win_height*font.cellh;

	if (term_resize(&term, &car, win_width, win_height) < 0)
		die(1, "failed to resize terminal");

	/* window */
	win = maus_init(win_title, 0, 0, winw, winh);
	if (!win)
		die(1, "failed to init window");
	if (!maus_create_window(win))
		die(1, "failed to create window");

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
		setenv("TERM", term_name, 1);

		/* shell */
		const char* sh = shell;
		if (sh == NULL)
			sh = getenv("SHELL");

		execl(sh, sh, "-i", (char *)NULL);

		_exit(127);
	}

	int flags = fcntl(term.ptyfd, F_GETFL, 0);
	if (flags >= 0) /* append to existing flags */
		fcntl(term.ptyfd, F_SETFL, flags|O_NONBLOCK);

	term_clear(&term, &car);
}

/* read from pty master */
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

/* write to pty master */
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

/* reload the font. true on success, false on failure.
   in event of failure, current font will still be in use  */
static bool reload_font(void)
{
	Fontface next;
	if (font_load(&next, font_path) < 0)
		return false;

	font_free(&font);
	font = next;
	resize_terminal(winw, winh);
	term_damage_all(&term);

	return true;
}

/* resize pty to reflect terminal values */
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

/* resize window framebuffer to reflect resized size */
static int resize_window(int w, int h)
{
	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;

	if (w == winw && h == winh)
		return 0;

	if (!maus_resize(win, w, h))
		return -1;
	winw = w;
	winh = h;

	return 0;
}

/* TODO */
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

/* event loop (TODO) */
static void run(void)
{
	MausEvent ev;
	Caret ocar = { -1, -1 };
	bool running = true;
	bool dirty = true;
	bool redraw_all = true;

	while (running) {
		while (maus_event_poll(win, &ev)) {
			if (ev.type == MAUS_EV_CLOSE) {
				running = false;
				break;
			}

			if (ev.type == MAUS_EV_RESIZE) {
				if (resize_window(ev.resize.width, ev.resize.height) < 0)
					die(EXIT_FAILURE, "failed to resize window");
				resize_terminal(ev.resize.width, ev.resize.height);
				term_damage_all(&term);
				dirty = true;
				redraw_all = true;
				continue;
			}

			if (ev.type == MAUS_EV_REDRAW) {
				dirty = true;
				continue;
			}

			if (handle_key(&ev)) {
				dirty = true;
				redraw_all = true;
			}
		}

		if (!running)
			break;

		if (ptyread())
			dirty = true;

		if (ocar.x != car.x || ocar.y != car.y) {
			term_damage_rune(&term, ocar.x, ocar.y);
			term_damage_rune(&term, car.x, car.y);
			dirty = true;
		}

		if (!dirty) {
			wait_events();
			continue;
		}

		if (redraw_all) {
			draw_clear(win->bfb, win->stride, winh, rgba(0, 0, 0, 255));
		}

		for (int y = 0; y < term.rows; y++) {
			for (int x = 0; x < term.cols; x++) {
				Rune* r = &RUNE(&term, x, y);
				bool cursor = term.cursor_visible &&
				              x == car.x && y == car.y;
				bool reverse = (r->attr & TERM_ATTR_REVERSE) != 0;
				uint32_t fg = reverse ? r->bg : r->fg;
				uint32_t bg = reverse ? r->fg : r->bg;

				if (cursor) {
					fg = cursor_fg;
					bg = cursor_bg;
				}

				if (!r->dmg)
					continue;

				draw_rune(
					win->bfb, win->stride, winh, x, y,
					font.cellw, font.cellh, bg
				);

				if (r->cp != ' ') {
					draw_codepoint(
						&font, win->bfb, win->stride, winh,
						x * font.cellw,
						y * font.cellh + (int)font.asc,
						r->cp,
						fg
					);
				}

				r->dmg = false;
			}
		}

		maus_target_fps(win, win_fps);
		maus_present(win);

		ocar = car;
		dirty = false;
		redraw_all = false;
	}
}

/* wait for activity while allowing window events */
static void wait_events(void)
{
	struct pollfd pfd = {
		.fd = term.ptyfd,
		.events = POLLIN,
	};

	if (term.ptyfd < 0) {
		struct timespec ts = {
			.tv_sec = 0,
			.tv_nsec = 16 * 1000 * 1000, /* TODO? libmaus */
		};
		nanosleep(&ts, NULL);
		return;
	}

	for (;;) {
		int ret = poll(&pfd, 1, 16);
		if (ret < 0 && errno == EINTR)
			continue;
		break;
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

