#ifndef MAUS_X11_H
#define MAUS_X11_H

#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

typedef enum {
	MAUS_ATOM_WM_DELETE_WINDOW,
	MAUS_ATOM_LAST
} MausX11Atoms;

typedef struct {
	Display* display;
	Window   root;
	Window   win;
	Atom     atoms[MAUS_ATOM_LAST];
	GC       gc;

	int8_t cur_rel; /* relative cursor */
	int8_t ignore_warp;
	int32_t mouse_x;
	int32_t mouse_y;
	int8_t  mouse_pos_set;

	XShmSegmentInfo shm;
	int8_t          shmat;
	XImage*         image; /* TODO: XImage path */
} MausBackend;

#endif /* MAUS_X11_H */

