#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#ifdef __linux__
#include <pty.h>
#else
#include <util.h>
#endif
#include <unistd.h>

#include <maus.h>
#include <maus_font.h>

#include "config.h"
#include "draw.h"
#include "term.h"
#include "utils.h"

static void cleanup(void);
static int font_cell_width(const MausFont* f);
static bool handle_key(const MausEvent* ev);
static void init(void);
static bool ptyread(void);
static void ptywrite(const char* s, size_t n);
static bool reload_font(void);
static void resize_pty(void);
static void resize_terminal(int w, int h);
static int resize_window(int w, int h);
static void run(void);
static bool shell_exited(void);
static void wait_events(void);

static MausFont* font;
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
	term_free(&term);
	maus_font_free(font);

	if (win)
		maus_close(win);
}

/* get terminal cell width for ascii glyphs */
static int font_cell_width(const MausFont* f)
{
	int width = 0;

	if (f->glyphs[0] && f->glyphs[0][0]) {
		for (int i = 32; i <= 126; i++) {
			const MausGlyph* g = &f->glyphs[0][0][i];
			if (g->valid && g->adv > width)
				width = g->adv;
		}
	}

	return width > 0 ? width : f->cellw;
}

/* dispatch correct keycode handling non ascii characters */
static bool handle_key(const MausEvent* ev)
{
	char text;

	if (ev->type != MAUS_EV_KEY || !ev->key.pressed)
		return false;

	if (BIND_RELOAD_FONT)
		return reload_font();

	if (BIND_PASTE) {
		char* text = maus_clipboard_get_text(win);

		if (text && text[0] != '\0') {
			/* bracketed paste mode on */
			if (term.bracketed_paste)
				ptywrite("\033[200~", 6);

			for (char* p = text; *p; p++) {
				/* replace newlines with carriage returns */
				char ch = (*p == '\n') ? '\r' : *p;
				ptywrite(&ch, 1);
			}

			/* bracketed paste mode off */
			if (term.bracketed_paste)
				ptywrite("\033[201~", 6);
		}

		return true;
	}

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
	const char* lang = getenv("LANG");
	if (!getenv("LC_ALL") && !getenv("LC_CTYPE") && (!lang || !*lang))
		setenv("LC_CTYPE", "C.UTF-8", 1);
	setlocale(LC_CTYPE, "");

	/* font */
	font = maus_font_load(font_path);
	if (!font)
		die(1, "failed to load font");
	term.cellw = font_cell_width(font);
	term.cellh = font->cellh;

	winw = win_width * term.cellw + 2 * padding;
	winh = win_height * term.cellh + 2 * padding;

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
	MausFont* next = maus_font_load(font_path);
	if (!next)
		return false;

	maus_font_free(font);
	font = next;
	term.cellw = font_cell_width(font);
	term.cellh = font->cellh;
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

/* TODO */
static void resize_terminal(int w, int h)
{
	int cols = (w - 2 * padding) / term.cellw;
	int rows = (h - 2 * padding) / term.cellh;

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
			if (ev.type == MAUS_EV_CLOSE || shell_exited()) {
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
				dirty = redraw_all = true;
				continue;
			}

			if (handle_key(&ev))
				dirty = true;
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
			maus_clear(win, MAUS_COL_ARGB((default_bg >> 24) & 0xff, (default_bg >> 16) & 0xff, (default_bg >> 8) & 0xff, default_bg & 0xff));
			term_damage_all(&term);
		}

		int grid_x = (winw - term.cols * term.cellw) / 2;
		int grid_y = (winh - term.rows * term.cellh) / 2;

		for (int y = 0; y < term.rows; y++) {
			for (int x = 0; x < term.cols; x++) {
				Rune* r = &RUNE(&term, x, y);
				if (r->width == 0) {
					r->dmg = false;
					continue;
				}
				bool wide = r->width == 2 && x + 1 < term.cols;
				bool cursor = term.cursor_visible && y == car.y &&
				              (x == car.x || (wide && x + 1 == car.x));
				bool reverse = (r->attr & TERM_ATTR_REVERSE) != 0;
				uint32_t fg = reverse ? r->bg : r->fg;
				uint32_t bg = reverse ? r->fg : r->bg;

				if (cursor) {
					uint32_t tmp = fg;
					fg = bg;
					bg = tmp;
				}

				if (!r->dmg && !(wide && RUNE(&term, x + 1, y).dmg))
					continue;

				draw_rune(
					win->bfb, win->stride, winh,
					grid_x + x * term.cellw,
					grid_y + y * term.cellh,
					term.cellw * (wide ? 2 : 1), term.cellh, bg
				);

				if (r->cp != ' ') {
					maus_draw_char(
						win, font,
						grid_x + x * term.cellw,
						grid_y + y * term.cellh,
						r->cp,
						MAUS_COL_ARGB((fg >> 24) & 0xff, (fg >> 16) & 0xff,
						              (fg >> 8) & 0xff, fg & 0xff)
					);
				}

				r->dmg = false;
				if (wide)
					RUNE(&term, x + 1, y).dmg = false;
			}
		}

		if (term.sixels_dirty) {
			for (size_t i = 0; i < term.sixels.count; i++) {
				SixelView* view = &term.sixels.views[i];
				draw_sixel(
					win->bfb, win->stride, winh, &view->image,
					grid_x + view->pos.x * term.cellw,
					grid_y + view->pos.y * term.cellh,
					grid_x, grid_y,
					grid_x + term.cols * term.cellw,
					grid_y + term.rows * term.cellh
				);
			}
			term.sixels_dirty = false;
		}
		maus_present(win);

		ocar = car;
		dirty = false;
		redraw_all = false;
	}
}

/* check if the shell has exited */
static bool shell_exited(void)
{
	int status;

	if (term.ptypid <= 0)
		return true;
	return waitpid(term.ptypid, &status, WNOHANG) == term.ptypid;
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
		int ret = poll(&pfd, 1, 1000 / polling_rate);
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

