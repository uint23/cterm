#ifndef MAUSWIN_H
#define MAUSWIN_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "maus_input.h"

/* auto selection */
#if (!defined(BACKEND_WIN) && !defined(BACKEND_MAC) && \
    !defined(BACKEND_X11) && !defined(BACKEND_WAY)) || \
    defined(BACKEND_AUTO)

#ifdef BACKEND_AUTO
#define MAUS_WARN_BACKEND_AUTO_SEL 0
#else
#define MAUS_WARN_BACKEND_AUTO_SEL 1
#endif /* BACKEND_AUTO */

	#if defined(_WIN32)
	#define BACKEND_WIN

	#elif defined(__APPLE__)
	#define BACKEND_MAC

	#else /* default to X */
	#define BACKEND_X11

	#endif
#else
#define MAUS_WARN_BACKEND_AUTO_SEL 0
#endif /* auto selection */

#if defined(BACKEND_X11)
	#include "maus_x11.h"

#elif defined(BACKEND_WAY)
	/* ... */

#elif defined(BACKEND_WIN)
	#include "maus_win.h"

#elif defined(BACKEND_MAC)
	/* ... */
#endif

#define MAUS_KEYCODE_LAST (256)
#define MAUS_COL_ARGB(a, r, g, b) (MausColor){a, r, g, b}
#define MAUS_COL_RGBA(r, g, b, a) (MausColor){a, r, g, b}
#define MAUS_UNPACK_COL(c) \
	(((uint32_t)(c).a << 24) | \
	 ((uint32_t)(c).r << 16) | \
	 ((uint32_t)(c).g <<  8) | \
	 ((uint32_t)(c).b))
/* ... */
#define MAUS_PIXEL_AT(mw, x, y) ((mw)->bfb[(y) * (mw)->stride + (x)])

typedef enum {
	MAUS_EV_NONE,
	MAUS_EV_CLOSE,
	MAUS_EV_KEY,
	MAUS_EV_MOUSE_BUTTON,
	MAUS_EV_MOUSE_MOTION,
	MAUS_EV_RESIZE,
} MausEventType;

typedef struct {
	uint8_t        a;
	uint8_t        r;
	uint8_t        g;
	uint8_t        b;
} MausColor;

typedef struct {
	MausEventType type;

	union {
		struct {
			uint32_t  code; /* raw, backend keycode */
			MausKey   key;  /* logical, mapped key */
			char      text; /* translated text from key */
			bool      pressed;
		} key;

		struct {
			struct {
				MausMouseButton button;
				bool            pressed;
			} button;

			struct {
				int32_t  x;
				int32_t  y;
			} motion;
		} mouse;

		struct {
			uint32_t width;
			uint32_t height;
		} resize;
	};
} MausEvent;

typedef struct {
	MausBackend    backend;
	uint64_t       frame_time_last;
	char*          clipboard;

	uint32_t*      fb;  /* front frame buffer */
	uint32_t*      bfb; /* back frame buffer */
	uint32_t       stride;

	const char*    title;
	uint32_t       width;
	uint32_t       height;
	int32_t        x;
	int32_t        y;

	bool           key_codes[MAUS_KEYCODE_LAST]; /* physical keys */
	bool           key_syms[MAUS_KEY_LAST];      /* logical keys */

	MausCursor     cursor;
	bool           mouse_buttons[MAUS_MOUSE_BUTTON_LAST];
} Maus;

/* clear screen with color: `col` */
void maus_clear(Maus* mw, MausColor col);

/* set text to system clipboard */
void maus_clipboard_set_text(Maus* mw, const char* text);

/* get text from system clipboard */
char* maus_clipboard_get_text(Maus* mw);

/* close a Maus. returns false on fail */
void maus_close(Maus* mw);

/* close a window without the whole Maus. returns false on fail */
bool maus_close_window(Maus* mw);

/* create window from Maus. returns false on fail */
bool maus_create_window(Maus* mw);

/* log message to output `fd` and die */
void maus_die(const char* fmt, ...);

/* get time since arbitrary start point (ns) */
uint64_t maus_get_time_ns(void);

/* initialise and fills the Maus. returns NULL on fail */
Maus* maus_init(const char* title, int x, int y, int width, int height);

/* log message to output `fd` */
void maus_log(FILE* fd, const char* fmt, ...);

/* poll for events then fill `ev` with retrieved events.
   returns true if event polled, else false.
   note: can burn cpu cycles */
bool maus_event_poll(Maus* mw, MausEvent* ev);

/* poll for events then fill `ev` with retrieved events. returns true if event
   polled, else false. note, thread goes to sleep until an event arrives */
void maus_event_wait(Maus* mw, MausEvent* ev);

/* present the pixelbuffer to the screen */
void maus_present(Maus* mw);

/* resize window to specified width and height. returns true on resize success,
   else false */
bool maus_resize(Maus* mw, uint32_t width, uint32_t height);

/* cap framerate to targetted fps */
void maus_target_fps(Maus* mw, uint32_t fps);

/* change behavior of mouse based on state passed */
void maus_cur_set_mode(Maus* mw, MausCursorState state);

#endif /* MAUSWIN_H */

