#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>

#include "maus.h"
#include "maus_input.h"
#include "maus_x11.h"
#include "utils.h"

typedef struct {
	KeySym  x11;
	MausKey maus;
} KeyMapEntry;

static bool fb_create(Maus* mw);
static bool fb_create_shm(Maus* mw);
/* TODO? static bool fb_create_ximage(Maus* mw); */
static void fb_destroy(Maus* mw);
static bool handle_event(XEvent* xev, MausEvent* ev, Maus* mw);
static MausKey keysym_to_mauskey(KeySym sym);
static MausMouseButton mouse_button_to_maus(int btn);
static int xerr(Display* dpy, XErrorEvent* ev);

static const KeyMapEntry keymap[] = {
	{ XK_BackSpace,    MAUS_KEY_BACKSPACE }, { XK_Tab,              MAUS_KEY_TAB },
	{ XK_Return,       MAUS_KEY_ENTER },     { XK_Escape,           MAUS_KEY_ESCAPE },
	{ XK_apostrophe,   MAUS_KEY_APOSTROPHE },{ XK_comma,            MAUS_KEY_COMMA },
	{ XK_space,        MAUS_KEY_SPACE },     { XK_minus,            MAUS_KEY_MINUS },
	{ XK_period,       MAUS_KEY_PERIOD },    { XK_slash,            MAUS_KEY_SLASH },
	{ XK_semicolon,    MAUS_KEY_SEMICOLON }, { XK_equal,            MAUS_KEY_EQUAL },
	{ XK_Delete,       MAUS_KEY_DELETE },    { XK_Left,             MAUS_KEY_LEFT },
	{ XK_Right,        MAUS_KEY_RIGHT },     { XK_Up,               MAUS_KEY_UP },
	{ XK_Down,         MAUS_KEY_DOWN },      { XK_Home,             MAUS_KEY_HOME },
	{ XK_End,          MAUS_KEY_END },       { XK_Page_Up,          MAUS_KEY_PAGE_UP },
	{ XK_Page_Down,    MAUS_KEY_PAGE_DOWN }, { XK_Insert,           MAUS_KEY_INSERT },
	{ XK_Shift_L,      MAUS_KEY_SHIFT_L },   { XK_Shift_R,          MAUS_KEY_SHIFT_R },
	{ XK_Control_L,    MAUS_KEY_CONTROL_L }, { XK_Control_R,        MAUS_KEY_CONTROL_R },
	{ XK_Alt_L,        MAUS_KEY_ALT_L },     { XK_Alt_R,            MAUS_KEY_ALT_R },
	{ XK_Super_L,      MAUS_KEY_SUPER_L },   { XK_Super_R,          MAUS_KEY_SUPER_R },
	{ XK_Caps_Lock,    MAUS_KEY_CAPS_LOCK }, { XK_Num_Lock,         MAUS_KEY_NUM_LOCK },
	{ XK_Pause,        MAUS_KEY_PAUSE },     { XK_Menu,             MAUS_KEY_MENU },
	{ XK_KP_Add,       MAUS_KEY_KP_ADD },    { XK_KP_Enter,         MAUS_KEY_KP_ENTER },
	{ XK_bracketright, MAUS_KEY_RIGHT_BRACKET }, { XK_grave,        MAUS_KEY_GRAVE },
	{ XK_Scroll_Lock,  MAUS_KEY_SCROLL_LOCK },   { XK_Print,        MAUS_KEY_PRINT_SCREEN },
	{ XK_KP_Decimal,   MAUS_KEY_KP_DECIMAL },    { XK_KP_Divide,    MAUS_KEY_KP_DIVIDE },
	{ XK_KP_Multiply,  MAUS_KEY_KP_MULTIPLY },   { XK_KP_Subtract,  MAUS_KEY_KP_SUBTRACT },
	{ XK_bracketleft,  MAUS_KEY_LEFT_BRACKET },  { XK_backslash,    MAUS_KEY_BACKSLASH },
	{ XK_KP_Equal,     MAUS_KEY_KP_EQUAL },
};

static int xerrored;

static bool fb_create(Maus* mw)
{
	MausBackend* be = &mw->backend;
	be->image = NULL;
	be->shmat = false;
	be->shm.shmid = -1;
	be->shm.shmaddr = NULL;
	mw->fb = NULL;
	mw->stride = 0;

	return fb_create_shm(mw);
}

/* allocate and store a new shm for the framebuffer */
static bool fb_create_shm(Maus* mw)
{
	MausBackend* be = &mw->backend;
	if (!XShmQueryExtension(be->display))
		return false;

	int scr = DefaultScreen(be->display);
	int depth = DefaultDepth(be->display, scr);
	Visual* vis = DefaultVisual(be->display, scr);
	if (!vis) {
		maus_log(stderr, "could not get default visual");
		return false;
	}

	be->image = XShmCreateImage(
		be->display, vis, depth, ZPixmap, NULL,
		&be->shm, mw->width, mw->height
	);
	if (!be->image) {
		maus_log(stderr, "could not create XImage (shm)");
		return false;
	}

	if (be->image->bits_per_pixel != 32) {
		XDestroyImage(be->image);
		be->image = NULL;
		maus_log(stderr, "XImage not 32bpp");
		return false;
	}

	size_t size = be->image->bytes_per_line * be->image->height;
	be->shm.shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
	if (be->shm.shmid < 0) {
		XDestroyImage(be->image);
		be->image = NULL;
		maus_log(stderr, "shmget() failed");
		return false;
	}

	be->shm.shmaddr = shmat(be->shm.shmid, NULL, 0);
	if (be->shm.shmaddr == (char*)-1) {
		shmctl(be->shm.shmid, IPC_RMID, NULL);
		XDestroyImage(be->image);
		be->image = NULL;
		maus_log(stderr, "shmat() failed");
		return false;
	}

	be->shm.readOnly = False;
	be->image->data = be->shm.shmaddr;

	xerrored = 0;
	int (*old_xerr)(Display*, XErrorEvent*) = XSetErrorHandler(xerr);

	if (!XShmAttach(be->display, &be->shm))
		xerrored = 1;

	XSync(be->display, False);
	XSetErrorHandler(old_xerr);
	if (xerrored) {
		shmdt(be->shm.shmaddr);
		shmctl(be->shm.shmid, IPC_RMID, NULL);
		XDestroyImage(be->image);

		be->image = NULL;
		mw->fb = NULL;
		be->shm.shmaddr = NULL;
		be->shm.shmid = -1;
		maus_log(stderr, "XShmAttach failed");
		return false;
	}

	shmctl(be->shm.shmid, IPC_RMID, NULL);

	be->shmat = true;
	mw->fb = (uint32_t*) be->image->data;
	mw->stride = be->image->bytes_per_line / sizeof(uint32_t);

	return true;
}

static void fb_destroy(Maus* mw)
{
	MausBackend* be = &mw->backend;
	Display* dpy = be->display;
	if (be->shmat) {
		XShmDetach(dpy, &be->shm);
		XSync(dpy, False);
		be->shmat = false;
	}

	if (be->image) {
		XDestroyImage(be->image);
		be->image = NULL;
	}

	if (be->shm.shmaddr && be->shm.shmaddr != (char*) -1) {
		shmdt(be->shm.shmaddr);
		be->shm.shmaddr = NULL;
	}

	/* normally already removed after attach but
	   can check anyways */
	if (be->shm.shmid >= 0) {
		shmctl(be->shm.shmid, IPC_RMID, NULL);
		be->shm.shmid = -1;
	}

	mw->fb = NULL;
	mw->stride = 0;
}

static bool handle_event(XEvent* xev, MausEvent* ev, Maus* mw)
{
	MausBackend* be = &mw->backend;
	uint32_t code = 0;
	unsigned int mb;
	MausMouseButton mbtype;

	switch (xev->type) {
		case ClientMessage:
			if ((Atom)xev->xclient.data.l[0] == be->atoms[MAUS_ATOM_WM_DELETE_WINDOW]) {
				ev->type = MAUS_EV_CLOSE;
				return true;
			}
			break;

		case MappingNotify:
			XRefreshKeyboardMapping(&xev->xmapping);
			return true;
			break;

		case KeyPress: {
			ev->type = MAUS_EV_KEY;
			code = xev->xkey.keycode;
			ev->key.code = code;
			ev->key.pressed = true;
			if (code < MAUS_KEYCODE_LAST)
			        mw->key_codes[code] = true;

			/* grab the raw, base sym */
			KeySym sym = XLookupKeysym(&xev->xkey, 0);
			ev->key.key = keysym_to_mauskey(sym);
			if (ev->key.key != MAUS_KEY_NONE)
			        mw->key_syms[ev->key.key] = true;

			char buf[8] = {0};
			int len = XLookupString(&xev->xkey, buf, sizeof(buf), NULL, NULL);
			if (len > 0)
			        ev->key.text = (buf[0] == '\r') ? '\n' : buf[0];
			else
			        ev->key.text = 0;

			return true;
		}

		case KeyRelease: {
			ev->type = MAUS_EV_KEY;
			code = xev->xkey.keycode;
			ev->key.code = code;
			ev->key.pressed = false;
			ev->key.text = 0;

			if (code < MAUS_KEYCODE_LAST)
			        mw->key_codes[code] = false;

			/* unset base sym set on `KeyPress` */
			KeySym sym = XLookupKeysym(&xev->xkey, 0);
			ev->key.key = keysym_to_mauskey(sym);
			if (ev->key.key != MAUS_KEY_NONE)
			        mw->key_syms[ev->key.key] = false;

			return true;
		}

		case ButtonPress:
			ev->type = MAUS_EV_MOUSE_BUTTON;
			mb = xev->xbutton.button;
			mbtype = mouse_button_to_maus(mb);
			ev->mouse.button.button = mbtype;
			ev->mouse.button.pressed = true;
			mw->mouse_buttons[mbtype] = true;
			return true;

		case ButtonRelease:
			ev->type = MAUS_EV_MOUSE_BUTTON;
			mb = xev->xbutton.button;
			mbtype = mouse_button_to_maus(mb);
			ev->mouse.button.button = mbtype;
			ev->mouse.button.pressed = false;
			mw->mouse_buttons[mbtype] = false;
			return true;

		case MotionNotify:
			ev->type = MAUS_EV_MOUSE_MOTION;
			ev->mouse.motion.x = xev->xmotion.x;
			ev->mouse.motion.y = xev->xmotion.y;
			return true;

		case ConfigureNotify:
			ev->type = MAUS_EV_RESIZE;
			ev->resize.width = xev->xconfigure.width;
			ev->resize.height = xev->xconfigure.height;
			return true;

		case SelectionRequest: {
			XSelectionRequestEvent* req = &xev->xselectionrequest;
			XEvent rsp; /* response */

			Atom utf8_string = XInternAtom(be->display, "UTF8_STRING", False);
			Atom clipboard = XInternAtom(be->display, "CLIPBOARD", False);

			rsp.type = SelectionNotify;
			rsp.xselection.display = req->display;
			rsp.xselection.requestor = req->requestor;
			rsp.xselection.selection = req->selection;
			rsp.xselection.target = req->target;
			rsp.xselection.property = None; 
			rsp.xselection.time = req->time;

			/* assign clipboard text to requesting programs
			   window property */
			if (req->selection == clipboard &&
			    req->target == utf8_string &&
			    mw->clipboard) {
			        XChangeProperty(
					req->display, req->requestor,
					req->property, utf8_string, 8,
					PropModeReplace, (unsigned char*)mw->clipboard, 
					strlen(mw->clipboard)
			 	);
				rsp.xselection.property = req->property; 
			}

			/* send reply back to the prog requesting paste */
			XSendEvent(req->display, req->requestor, False, 0, &rsp);
			XFlush(be->display);

			return false; /* skip maus polling it */
		}
	}

	return false;
}

static MausKey keysym_to_mauskey(KeySym sym)
{
	if (sym >= 0x20 && sym <= 0x7E)
		return (MausKey)sym;

	if (sym >= XK_F1 && sym <= XK_F12)
		return MAUS_KEY_F1 + (sym - XK_F1);
	if (sym >= XK_KP_0 && sym <= XK_KP_9)
		return MAUS_KEY_KP_0 + (sym - XK_KP_0);

	for (size_t i = 0; i < sizeof(keymap)/ sizeof(keymap[0]); i++) {
		if (keymap[i].x11 == sym)
			return keymap[i].maus;
	}

	return MAUS_KEY_NONE;
}

static MausMouseButton mouse_button_to_maus(int btn)
{
	switch (btn) {
		case Button1: return MAUS_MOUSE_BUTTON_LEFT;
		case Button2: return MAUS_MOUSE_BUTTON_MIDDLE;
		case Button3: return MAUS_MOUSE_BUTTON_RIGHT;
		case Button4: return MAUS_MOUSE_BUTTON_SCROLL_UP;
		case Button5: return MAUS_MOUSE_BUTTON_SCROLL_DOWN;
		default:      return MAUS_MOUSE_BUTTON_NONE;
	}
}

static int xerr(Display* dpy, XErrorEvent* ev)
{
	(void) dpy;
	(void) ev;
	xerrored = 1;
	return 0;
}

void maus_clipboard_set_text(Maus* mw, const char* text)
{
	MausBackend* be = &mw->backend;
	if (!be->display || be->win == None)
		return;

	/* clear old entry */
	if (mw->clipboard) {
		free(mw->clipboard);
		mw->clipboard = NULL;
	}

	if (text)
		mw->clipboard = maus_strdup(text);

	Atom clipboard = XInternAtom(be->display, "CLIPBOARD", False);
	XSetSelectionOwner(be->display, clipboard, be->win, CurrentTime);
	XFlush(be->display);
}

char* maus_clipboard_get_text(Maus* mw)
{
	MausBackend* be = &mw->backend;
	Atom clipboard = XInternAtom(be->display, "CLIPBOARD", False);
	Atom utf8_string = XInternAtom(be->display, "UTF8_STRING", False);
	Atom maus_clipboard_prop = XInternAtom(be->display, "MAUS_CLIPBOARD_PROP", False);

	/* fetch clipboard data to `maus_clipboard_prop`
	   window property */
	XConvertSelection(
		be->display, clipboard, utf8_string,
		maus_clipboard_prop, be->win, CurrentTime
	);
	XFlush(be->display);

	/* check for selection response */
	XEvent xev;
	bool ok = false;
	for (int timeout = 0; timeout < 10000; timeout++) { 
		if (XCheckTypedWindowEvent(be->display, be->win, SelectionNotify, &xev)) {
			if (xev.xselection.property != None)
				ok = true;
			break;
		}
	}
	if (!ok)
		return NULL;

	/* read string off property */
	Atom actual_type;
	int actual_fmt;
	unsigned long nitems;
	unsigned long bytes_after;
	unsigned char* prop_data = NULL;
	XGetWindowProperty(
		be->display, be->win, maus_clipboard_prop, 0,LONG_MAX,
		True, utf8_string, &actual_type, &actual_fmt, &nitems,
		&bytes_after, &prop_data
	);

	char* res = NULL;
	if (prop_data) {
		res = maus_strdup((char*)prop_data);
		XFree(prop_data);
	}

	return res;
}

void maus_close(Maus* mw)
{
	MausBackend* be = &mw->backend;
	fb_destroy(mw);
	if (mw->bfb) {
		free(mw->bfb);
		mw->bfb = NULL;
	}
	if (mw->clipboard) {
		free(mw->clipboard);
		mw->clipboard = NULL;
	}
	if (be->win != None) {
		(void) maus_close_window(mw);
	}
	if (be->display)
		XCloseDisplay(be->display);
}

bool maus_close_window(Maus* mw)
{
	MausBackend* be = &mw->backend;
	if (be->gc) {
		XFreeGC(be->display, be->gc);
		be->gc = 0;
	}

	if (be->win != None) {
		XUnmapWindow(be->display, be->win);
		XDestroyWindow(be->display, be->win);
	}

	return true;
}

bool maus_create_window(Maus* mw)
{
	MausBackend* be = &mw->backend;
	Display* dpy = be->display;
	Window* win = &be->win;

	*win = XCreateSimpleWindow(
		dpy, be->root,
		mw->x, mw->y,
		mw->width, mw->height,
		0u, 0u, 0u
	);

	if (*win == None)
		return false;

	XSelectInput(dpy, *win,
		ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
		ButtonReleaseMask | PointerMotionMask | StructureNotifyMask
	);
	/* no black flashing when resizing */
	XSetWindowBackgroundPixmap(dpy, *win, None);

	/* atoms */
	be->atoms[MAUS_ATOM_WM_DELETE_WINDOW] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

	XSetWMProtocols(dpy, *win, &be->atoms[MAUS_ATOM_WM_DELETE_WINDOW], 1);
	XStoreName(dpy, *win, mw->title);

	XMapWindow(dpy, *win);
	XFlush(dpy);

	be->gc = XCreateGC(dpy, *win, 0, NULL);
	if (!be->gc) {
		maus_log(stderr, "failed to create window GC");
		maus_close_window(mw);
		return false;
	}

	return true;
}

void maus_clear(Maus* mw, MausColor col)
{
	uint32_t col_up = MAUS_UNPACK_COL(col);
	uint32_t pxs = mw->height * mw->width;
	for (uint32_t i = 0; i < pxs; i++)
		mw->bfb[i] = col_up;
}

Maus* maus_init(const char* title, int x, int y, int width, int height)
{
	if (MAUS_WARN_BACKEND_AUTO_SEL)
		maus_log(stderr, "backend auto-selected: X11");

	Display* d = XOpenDisplay(NULL);
	if (!d) {
		maus_die("failed to open X11 display");
		return NULL;
	}

	Maus* mw = calloc(1, sizeof(Maus));
	MausBackend* be = &mw->backend;
	be->display = d;
	be->root = DefaultRootWindow(d);
	be->win = None;
	mw->frame_time_last = maus_get_time_ns();
	mw->title = title;
	mw->width = width;
	mw->height = height;
	mw->x = x;
	mw->y = y;
	mw->fb = NULL;

	be->gc = 0;
	be->image = NULL;
	be->shmat = false;
	be->shm.shmid = -1;
	be->shm.shmaddr = NULL;
	if (!fb_create(mw)) {
		maus_log(stderr, "failed to create framebuffer");
		maus_close(mw);
		free(mw);
		return NULL;
	}
	mw->bfb = calloc(mw->stride * mw->height, sizeof(uint32_t));
	if (!mw->bfb) {
		maus_log(stderr, "failed to allocate back buffer");
		maus_close(mw);
		free(mw);
		return NULL;
	}

	return mw;
}

bool maus_event_poll(Maus* mw, MausEvent* ev)
{
	MausBackend* be = &mw->backend;
	XEvent xev;
	while (XPending(be->display)) {
		XNextEvent(be->display, &xev);
		ev->type = MAUS_EV_NONE;
		if (handle_event(&xev, ev, mw))
			return true;

	}

	return false;
}

void maus_event_wait(Maus* mw, MausEvent* ev)
{
	MausBackend* be = &mw->backend;
	XEvent xev;
	for (;;) {
		XNextEvent(be->display, &xev);
		ev->type = MAUS_EV_NONE;
		if (handle_event(&xev, ev, mw))
			return;
	}
}

void maus_present(Maus* mw)
{
	MausBackend* be = &mw->backend;
	if (!be->image || be->win == None)
		return;

	uint32_t bytes = mw->stride * mw->height * sizeof(uint32_t);
	memcpy(mw->fb, mw->bfb, bytes);

	XShmPutImage(
		be->display, be->win, be->gc, be->image,
		0, 0, 0, 0, mw->width, mw->height, False
	);

	XFlush(be->display);
}

bool maus_resize(Maus* mw, uint32_t width, uint32_t height)
{
	MausBackend* be = &mw->backend;
	if (width == 0 || height == 0 ||
	    (mw->width == width && mw->height == height))
		return false;

	fb_destroy(mw);
	if (mw->bfb) {
		free(mw->bfb);
		mw->bfb = NULL;
	}

	mw->width = width;
	mw->height = height;

	XSync(be->display, False);
	XFlush(be->display);

	if (!fb_create(mw))
		return false;

	mw->bfb = calloc(mw->stride * mw->height, sizeof(uint32_t));
	if (!mw->bfb)
		return false;

	return true;
}

void maus_cur_set_mode(Maus* mw, MausCursorState state)
{
	MausBackend* be = &mw->backend;
	Display* dpy = be->display;
	Window win = be->win;

	if (!dpy) {
		maus_log(stderr, "display is NULL");
		return;
	}
	if (win == None) {
		maus_log(stderr, "win is None");
		return;
	}

	/* show cursor */
	if (state == MAUS_CURSOR_STATE_VISIBLE) {
		XUndefineCursor(dpy, win);
		XFlush(dpy);
		return;
	}

	/* hide cursor */
	if (state == MAUS_CURSOR_STATE_HIDDEN) {
		Pixmap blank_pm = XCreatePixmap(dpy, win, 1, 1, 1);
		XColor black = {.red=0, .green=0, .blue=0};
		Cursor blank_cur = XCreatePixmapCursor(
			dpy, blank_pm, blank_pm, &black, &black, 0, 0
		);
		XFreePixmap(dpy, blank_pm);

		XDefineCursor(dpy, win, blank_cur);
		XFreeCursor(dpy, blank_cur);
		XFlush(dpy);
		return;
	}

	/* lock cursor */
	if (state == MAUS_CURSOR_STATE_LOCKED) {
		Mask mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
		int grab = XGrabPointer(
			dpy, win, True, mask, GrabModeAsync,
			GrabModeAsync, win, None, CurrentTime
		);

		if (grab != GrabSuccess) {
			maus_log(stderr, "failed to grab mouse pointer");
			return;
		}

		/* TODO keep position just snap to window dimensions */
		XWarpPointer(dpy, None, win, 0, 0, 0, 0, mw->width/2, mw->height/2);
		XFlush(dpy);
		return;
	}

	/* unlock cursor */
	if (state == MAUS_CURSOR_STATE_FREE) {
		XUngrabPointer(dpy, CurrentTime);
		XFlush(dpy);
		return;
	}
}

