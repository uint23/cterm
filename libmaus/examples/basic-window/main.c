#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "maus.h"
#include "maus_font.h"

int mx, my;
bool cur_visible = true;
bool cur_locked = false;
bool cur_relative = false;
bool mb1_pressed = false;
char txtbuf[4096] = {'\0'};
int current_char = 0;
bool resize = false;
int rszw;
int rszh;
MausColor white = {255, 255, 255, 255};

void handle_ev(Maus* mw, MausEvent* ev)
{
	switch (ev->type) {
		case MAUS_EV_CLOSE:
			maus_close(mw);
			exit(EXIT_SUCCESS);
			break;
		case MAUS_EV_KEY: {
			(void)0;

			int8_t* keys = mw->key_syms;
			if (keys[MAUS_KEY_CONTROL_L] && keys[MAUS_KEY_Q]) {
				maus_close(mw);
				exit(EXIT_SUCCESS);
			};
			if (keys[MAUS_KEY_CONTROL_L] && keys[MAUS_KEY_P]) {
				cur_visible ?
				maus_cur_set_mode(mw, MAUS_CURSOR_STATE_HIDDEN) :
				maus_cur_set_mode(mw, MAUS_CURSOR_STATE_VISIBLE);

				cur_visible = !cur_visible;
			}
			if (keys[MAUS_KEY_CONTROL_L] && keys[MAUS_KEY_L]) {
				cur_locked ?
				maus_cur_set_mode(mw, MAUS_CURSOR_STATE_FREE) :
				maus_cur_set_mode(mw, MAUS_CURSOR_STATE_LOCKED);

				cur_locked = !cur_locked;
			}
			if (keys[MAUS_KEY_CONTROL_L] && keys[MAUS_KEY_R]) {
				cur_relative ?
				maus_cur_set_mode(mw, MAUS_CURSOR_STATE_ABSOLUTE) :
				maus_cur_set_mode(mw, MAUS_CURSOR_STATE_RELATIVE);

				cur_relative = !cur_relative;
			}

			if (ev->key.pressed == false)
				break;

			if (!((ev->key.key >= 32 && ev->key.key <= 126) || ev->key.key == '\n'))
				break;

			if (current_char + 1 < 4095 && current_char >= 0)
				txtbuf[current_char++] = ev->key.text;
			else
				maus_log(stderr, "txtbuf full");

			break;
		}
		case MAUS_EV_MOUSE_BUTTON: {
			if (ev->mouse.button.button == MAUS_MOUSE_BUTTON_SCROLL_UP) {
				white.r += 5;
				white.g += 5;
				white.b += 5;
			}
			else if (ev->mouse.button.button == MAUS_MOUSE_BUTTON_SCROLL_DOWN) {
				white.r -= 5;
				white.g -= 5;
				white.b -= 5;
			}
			mb1_pressed =
			mw->mouse_buttons[MAUS_MOUSE_BUTTON_LEFT] ?
			true : false;
			break;
		}
		case MAUS_EV_MOUSE_MOTION: {
			mx = ev->mouse.motion.x;
			my = ev->mouse.motion.y;

		} break;
		case MAUS_EV_RESIZE: {
			resize = true;
			rszw = ev->resize.width;
			rszh = ev->resize.height;
		} break;
		case MAUS_EV_REDRAW:
			break;
		case MAUS_EV_NONE:
			break;
	}
}

int main(void)
{
	Maus* mw = maus_init("basic window", 0, 0, 800, 600);
	if (!mw)
		return EXIT_FAILURE;
	maus_create_window(mw);
	MausFont* font = maus_font_load("assets/fonts/font1.bdf");
	if (!font)
		return EXIT_FAILURE;

	MausEvent ev;
	MausColor red = { 255, 255, 0, 0 };
	/* unsigned long long ticks = 0; */
	for (;;) {
		while (maus_event_poll(mw, &ev))
			(void) handle_ev(mw, &ev);

		/* only resize window once per fram */
		if (resize) {
			maus_resize(mw, rszw, rszh);
			resize = false;
		}

		maus_clear(mw, white);

		uint32_t red_unpacked = MAUS_UNPACK_COL(red);
		if (mx >= 0 && my >= 0 &&
		    mx < (int32_t)mw->width &&
		    my < (int32_t)mw->height && mb1_pressed) {
			MAUS_PIXEL_AT(mw, mx, my) = red_unpacked;
		}

		for (uint32_t y = 0; y < mw->height/2; y++) {
			for (uint32_t x = 0; x < mw->width/2; x++) {
				MAUS_PIXEL_AT(mw, x, y) = red_unpacked;
			}
		}
		red.b++;

		/* maus_draw_text(mw, font, 700, 50, "maus font\nloading example", red); */

		/* char buf[512] = {0}; */
		/* snprintf(buf, 512, "maus' basic-window has been running for %lld ticks!", ticks); */
		/* maus_clipboard_set_text(mw, buf); */

		maus_draw_text(mw, font, 700, 50, txtbuf, red);

		maus_target_fps(mw, 60);
		maus_present(mw);
	}
	maus_font_free(font);
	maus_close(mw);
}
