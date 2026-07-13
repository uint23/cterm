#ifndef MAUS_X11_H
#define MAUS_X11_H

#include <stdbool.h>

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

typedef enum {
	MAUS_ATOM_WM_DELETE_WINDOW,
	MAUS_ATOM_LAST,
} MausX11Atoms;

typedef struct {
	Display*       display;
	Window         root;
	Window         win;
	Atom           atoms[MAUS_ATOM_LAST];
	GC             gc;

	XImage*        image;
	XShmSegmentInfo shm;
	bool           shmat;
} MausBackend;

#endif /* MAUS_X11_H */

