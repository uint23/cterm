#include <windows.h>
#include <windowsx.h>

#include "maus.h"
#include "maus_input.h"
#include "maus_win.h"

typedef struct {
	UINT    vk;
	MausKey maus;
} KeyMapEntry;

static bool fb_create(Maus* mw);
static void fb_destroy(Maus* mw);
static bool handle_event(const MSG* msg, MausEvent* ev, Maus* mw);
static MausKey vk_to_mauskey(UINT vk);
static UINT resolve_lr(UINT vk, LPARAM lparam);
static char translate_text(UINT vk, UINT scan);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static const KeyMapEntry keymap[] = {
	{ VK_BACK,     MAUS_KEY_BACKSPACE }, { VK_TAB,      MAUS_KEY_TAB },
	{ VK_RETURN,   MAUS_KEY_ENTER },     { VK_ESCAPE,   MAUS_KEY_ESCAPE },
	{ VK_SPACE,    MAUS_KEY_SPACE },     { VK_DELETE,   MAUS_KEY_DELETE },
	{ VK_LEFT,     MAUS_KEY_LEFT },      { VK_RIGHT,    MAUS_KEY_RIGHT },
	{ VK_UP,       MAUS_KEY_UP },        { VK_DOWN,     MAUS_KEY_DOWN },
	{ VK_HOME,     MAUS_KEY_HOME },      { VK_END,      MAUS_KEY_END },
	{ VK_PRIOR,    MAUS_KEY_PAGE_UP },   { VK_NEXT,     MAUS_KEY_PAGE_DOWN },
	{ VK_INSERT,   MAUS_KEY_INSERT },    { VK_CAPITAL,  MAUS_KEY_CAPS_LOCK },
	{ VK_NUMLOCK,  MAUS_KEY_NUM_LOCK },  { VK_SCROLL,   MAUS_KEY_SCROLL_LOCK },
	{ VK_PAUSE,    MAUS_KEY_PAUSE },     { VK_SNAPSHOT, MAUS_KEY_PRINT_SCREEN },
	{ VK_APPS,     MAUS_KEY_MENU },      { VK_LSHIFT,   MAUS_KEY_SHIFT_L },
	{ VK_RSHIFT,   MAUS_KEY_SHIFT_R },   { VK_LCONTROL, MAUS_KEY_CONTROL_L },
	{ VK_RCONTROL, MAUS_KEY_CONTROL_R }, { VK_LMENU,    MAUS_KEY_ALT_L },
	{ VK_RMENU,    MAUS_KEY_ALT_R },     { VK_LWIN,     MAUS_KEY_SUPER_L },
	{ VK_RWIN,     MAUS_KEY_SUPER_R },   { VK_ADD,      MAUS_KEY_KP_ADD },
	{ VK_SUBTRACT, MAUS_KEY_KP_SUBTRACT },{ VK_MULTIPLY, MAUS_KEY_KP_MULTIPLY },
	{ VK_DIVIDE,   MAUS_KEY_KP_DIVIDE }, { VK_DECIMAL,  MAUS_KEY_KP_DECIMAL },
};

static bool fb_create(Maus* mw)
{
	MausBackend* be = &mw->backend;
	be->hbm = NULL;
	mw->fb = NULL;
	mw->stride = 0;

	BITMAPINFO bmi = {0};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = mw->width;
	bmi.bmiHeader.biHeight = -mw->height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	HDC dc = GetDC(NULL);
	be->memdc = CreateCompatibleDC(dc);
	be->hbm = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, (void**)&mw->fb, NULL, 0);
	ReleaseDC(NULL, dc);

	if (!be->hbm || !mw->fb) {
		if (be->memdc)
			DeleteDC(be->memdc);
		maus_log(stderr, "CreateDIBSection failed");
		return false;
	}

	SelectObject(be->memdc, be->hbm);

	mw->stride = mw->width;
	return true;
}

static void fb_destroy(Maus* mw)
{
	MausBackend* be = &mw->backend;

	if (be->memdc) {
		DeleteDC(be->memdc);
		be->memdc = NULL;
	}
	if (be->hbm) {
		DeleteObject(be->hbm);
		be->hbm = NULL;
	}

	mw->fb = NULL;
	mw->stride = 0;
}

static bool handle_event(const MSG* msg, MausEvent* ev, Maus* mw)
{
	UINT scan;
	UINT vk;
	int delta;
	MausMouseButton mbtype;

	switch (msg->message) {
		case WM_QUIT:
			ev->type = MAUS_EV_CLOSE;
			return true;

		case WM_MOUSEMOVE:
			ev->type = MAUS_EV_MOUSE_MOTION;
			ev->mouse.motion.x = GET_X_LPARAM(msg->lParam);
			ev->mouse.motion.y = GET_Y_LPARAM(msg->lParam);
			return true;

		case WM_LBUTTONDOWN:
			ev->type = MAUS_EV_MOUSE_BUTTON;
			ev->mouse.button.button = MAUS_MOUSE_BUTTON_LEFT;
			ev->mouse.button.pressed = true;
			mw->mouse_buttons[MAUS_MOUSE_BUTTON_LEFT] = true;
			return true;
		case WM_LBUTTONUP:
			ev->type = MAUS_EV_MOUSE_BUTTON;
			ev->mouse.button.button = MAUS_MOUSE_BUTTON_LEFT;
			ev->mouse.button.pressed = false;
			mw->mouse_buttons[MAUS_MOUSE_BUTTON_LEFT] = false;
			return true;

		case WM_RBUTTONDOWN:
			ev->type = MAUS_EV_MOUSE_BUTTON;
			ev->mouse.button.button = MAUS_MOUSE_BUTTON_RIGHT;
			ev->mouse.button.pressed = true;
			mw->mouse_buttons[MAUS_MOUSE_BUTTON_RIGHT] = true;
			return true;
		case WM_RBUTTONUP:
			ev->type = MAUS_EV_MOUSE_BUTTON;
			ev->mouse.button.button = MAUS_MOUSE_BUTTON_RIGHT;
			ev->mouse.button.pressed = false;
			mw->mouse_buttons[MAUS_MOUSE_BUTTON_RIGHT] = false;
			return true;

		case WM_MBUTTONDOWN:
			ev->type = MAUS_EV_MOUSE_BUTTON;
			ev->mouse.button.button = MAUS_MOUSE_BUTTON_MIDDLE;
			ev->mouse.button.pressed = true;
			mw->mouse_buttons[MAUS_MOUSE_BUTTON_MIDDLE] = true;
			return true;
		case WM_MBUTTONUP:
			ev->type = MAUS_EV_MOUSE_BUTTON;
			ev->mouse.button.button = MAUS_MOUSE_BUTTON_MIDDLE;
			ev->mouse.button.pressed = false;
			mw->mouse_buttons[MAUS_MOUSE_BUTTON_MIDDLE] = false;
			return true;

		case WM_MOUSEWHEEL:
			delta = GET_WHEEL_DELTA_WPARAM(msg->wParam);
			mbtype = (delta > 0) ? MAUS_MOUSE_BUTTON_SCROLL_UP
			                     : MAUS_MOUSE_BUTTON_SCROLL_DOWN;
			ev->type = MAUS_EV_MOUSE_BUTTON;
			ev->mouse.button.button = mbtype;
			ev->mouse.button.pressed = true;
			return true;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			scan = (UINT)((msg->lParam >> 16) & 0xFF);
			vk = resolve_lr((UINT)msg->wParam, msg->lParam);

			ev->type = MAUS_EV_KEY;
			ev->key.code = (uint32_t)msg->wParam;
			ev->key.pressed = true;
			ev->key.key = vk_to_mauskey(vk);
			ev->key.text = translate_text((UINT)msg->wParam, scan);

			if (ev->key.code < MAUS_KEYCODE_LAST)
				mw->key_codes[ev->key.code] = true;
			if (ev->key.key != MAUS_KEY_NONE)
				mw->key_syms[ev->key.key] = true;
			return true;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			vk = resolve_lr((UINT)msg->wParam, msg->lParam);

			ev->type = MAUS_EV_KEY;
			ev->key.code = (uint32_t)msg->wParam;
			ev->key.pressed = false;
			ev->key.key = vk_to_mauskey(vk);
			ev->key.text = 0;

			if (ev->key.code < MAUS_KEYCODE_LAST)
				mw->key_codes[ev->key.code] = false;
			if (ev->key.key != MAUS_KEY_NONE)
				mw->key_syms[ev->key.key] = false;
			return true;
	}

	return false;
}

static MausKey vk_to_mauskey(UINT vk)
{
	if (vk >= 'A' && vk <= 'Z')
		return (MausKey)(MAUS_KEY_A + (vk - 'A'));
	if (vk >= '0' && vk <= '9')
		return (MausKey)(MAUS_KEY_0 + (vk - '0'));
	if (vk >= VK_F1 && vk <= VK_F12)
		return (MausKey)(MAUS_KEY_F1 + (vk - VK_F1));
	if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
		return (MausKey)(MAUS_KEY_KP_0 + (vk - VK_NUMPAD0));

	for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
		if (keymap[i].vk == vk)
			return keymap[i].maus;
	}

	return MAUS_KEY_NONE;
}

static UINT resolve_lr(UINT vk, LPARAM lparam)
{
	UINT scan = (UINT)((lparam >> 16) & 0xFF);
	int  ext  = (lparam & (1 << 24)) != 0;

	if (vk == VK_SHIFT)
		return MapVirtualKey(scan, MAPVK_VSC_TO_VK_EX);
	if (vk == VK_CONTROL)
		return ext ? VK_RCONTROL : VK_LCONTROL;
	if (vk == VK_MENU)
		return ext ? VK_RMENU : VK_LMENU;

	return vk;
}

static char translate_text(UINT vk, UINT scan)
{
	BYTE state[256];
	if (!GetKeyboardState(state))
		return 0;

	WCHAR buf[4];
	int n = ToUnicode(vk, scan, state, buf, 4, 0);
	if (n == 1 && buf[0] < 0x80)
		return (buf[0] == '\r') ? '\n' : (char)buf[0];

	return 0;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (msg == WM_DESTROY) {
		PostQuitMessage(EXIT_SUCCESS);
		return 0;
	}

	if (msg == WM_SIZE) {
		Maus* mw = (Maus*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (mw) {
			MausBackend* be = &mw->backend;
			be->resized = true;
			be->rw = LOWORD(lp);
			be->rh = HIWORD(lp);
		}
		return 0;
	}

	return DefWindowProc(hwnd, msg, wp, lp);
}

void maus_clear(Maus* mw, MausColor col)
{
	uint32_t col_up = MAUS_UNPACK_COL(col);
	uint32_t pxs = mw->height * mw->width;
	for (uint32_t i = 0; i < pxs; i++)
		mw->bfb[i] = col_up;
}

void maus_clipboard_set_text(Maus* mw, const char* text)
{
	MausBackend* be = &mw->backend;
	if (!text || !OpenClipboard(be->hwnd))
		return;
	EmptyClipboard();
	size_t len = strlen(text) + 1;
	HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, len);
	if (hmem) {
		void* ptr = GlobalLock(hmem);
		if (ptr) {
			memcpy(ptr, text, len);
			GlobalUnlock(hmem);

			if (!SetClipboardData(CF_TEXT, hmem))
				GlobalFree(hmem); /* failed */
		}
		else {
			GlobalFree(hmem);
		}
	}

	CloseClipboard();
}

char* maus_clipboard_get_text(Maus* mw)
{
	MausBackend* be = &mw->backend;
	if (!OpenClipboard(be->hwnd))
		return NULL;

	HANDLE hmem = GetClipboardData(CF_TEXT);
	if (!hmem) {
		CloseClipboard();
		return NULL;
	}

	const char* text = GlobalLock(hmem);
	if (text) {
		if (mw->clipboard) {
			free(mw->clipboard);
			mw->clipboard = NULL;
		}

		size_t len = strlen(text) + 1;
		mw->clipboard = malloc(len);
		if (mw->clipboard)
			memcpy(mw->clipboard, text, len);

		GlobalUnlock(hmem);
	}

	CloseClipboard();
	return mw->clipboard;
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
	if (be->hwnd)
		(void) maus_close_window(mw);
}

bool maus_close_window(Maus* mw)
{
	MausBackend* be = &mw->backend;

	if (be->hwnd) {
		DestroyWindow(be->hwnd);
		be->hwnd = NULL;
		return true;
	}

	return false;
}


bool maus_create_window(Maus* mw)
{
	MausBackend* be = &mw->backend;
	be->hInst = GetModuleHandle(NULL);

	WNDCLASS wc = {0};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = be->hInst;
	wc.lpszClassName = "MausWindow";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	if (!RegisterClass(&wc))
		return false;

	RECT r = { 0, 0, mw->width, mw->height };
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

	be->hwnd = CreateWindowEx(
		0, "MausWindow", mw->title,
		WS_OVERLAPPEDWINDOW, mw->x, mw->y,
		r.right - r.left, r.bottom - r.top,
		NULL, NULL, be->hInst, NULL
	);

	if (!be->hwnd)
		return false;

	SetWindowLongPtr(be->hwnd, GWLP_USERDATA, (LONG_PTR)mw);
	ShowWindow(be->hwnd, SW_SHOW);
	UpdateWindow(be->hwnd);
	return true;
}

Maus* maus_init(const char* title, int x, int y, int width, int height)
{
	Maus* mw = calloc(1, sizeof(Maus));
	MausBackend* be = &mw->backend;

	mw->frame_time_last = maus_get_time_ns();
	mw->title = title;
	mw->width = width;
	mw->height = height;
	mw->x = x;
	mw->y = y;
	be->hInst = GetModuleHandle(NULL);
	be->hwnd = NULL;
	be->hbm = NULL;

	if (!fb_create(mw)) {
		maus_log(stderr, "failed to create framebuffer");
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
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		ev->type = MAUS_EV_NONE;

		if (handle_event(&msg, ev, mw))
			return true;
	}

	/* WM_SIZE doesnt is handled separately */
	if (be->resized) {
		be->resized = false;
		ev->type = MAUS_EV_RESIZE;
		ev->resize.width = be->rw;
		ev->resize.height = be->rh;
		return true;
	}

	return false;
}

void maus_event_wait(Maus* mw, MausEvent* ev)
{
	MausBackend* be = &mw->backend;
	MSG msg;

	for (;;) {
		int r = GetMessage(&msg, NULL, 0, 0);
		if (r <= 0) {
			ev->type = MAUS_EV_CLOSE;
			return;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
		ev->type = MAUS_EV_NONE;

		/* WM_SIZE doesnt is handled separately */
                if (be->resized) {
			be->resized = false;
			ev->type = MAUS_EV_RESIZE;
			ev->resize.width = be->rw;
			ev->resize.height = be->rh;
			return;
		}

		if (handle_event(&msg, ev, mw))
			return;
	}
}

void maus_present(Maus* mw)
{
	MausBackend* be = &mw->backend;
	if (!be->hwnd || !be->memdc)
		return;
	HDC wdc = GetDC(be->hwnd);

	uint32_t bytes = mw->stride * mw->height * sizeof(uint32_t);
	memcpy(mw->fb, mw->bfb, bytes);

	BitBlt(wdc, 0, 0, mw->width, mw->height, be->memdc, 0, 0, SRCCOPY);
	ReleaseDC(be->hwnd, wdc);
}

bool maus_resize(Maus* mw, uint32_t width, uint32_t height)
{
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
	HWND hwnd = be->hwnd;

	if (!hwnd) {
		maus_log(stderr, "hwnd is NULL");
		return;
	}

	/* show cursor */
	if (state == MAUS_CURSOR_STATE_VISIBLE) {
		while (ShowCursor(TRUE) < 0);
		return;
	}

	/* hide cursor */
	if (state == MAUS_CURSOR_STATE_HIDDEN) {
		while (ShowCursor(FALSE) >= 0);
		return;
	}


	/* lock cursor */
	if (state == MAUS_CURSOR_STATE_LOCKED) {
		RECT r;
		GetClientRect(hwnd, &r);
		MapWindowPoints(hwnd, NULL, (POINT*)&r, 2);

		if (!ClipCursor(&r)) {
			maus_log(stderr, "failed to grab mouse pointer");
			return;
		}

		return;
	}

	/* unlock cursor */
	if (state == MAUS_CURSOR_STATE_FREE) {
		ClipCursor(NULL);
		return;
	}
}

