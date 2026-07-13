#ifndef MAUS_WIN_H
#define MAUS_WIN_H

#include <stdbool.h>
#include <stdint.h>

#include <windows.h>

typedef struct {
	HWND           hwnd;
	HINSTANCE      hInst;
	HDC            hdc;
	HBITMAP        hbm;
	HDC            memdc;
	bool           resized;
	uint32_t       rw;
	uint32_t       rh;
} MausBackend;

#endif /* MAUS_WIN_H */

