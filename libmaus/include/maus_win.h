#ifndef MAUS_WIN_H
#define MAUS_WIN_H

#include <stdint.h>

#include <windows.h>

typedef struct {
	HWND      hwnd;
	HINSTANCE hInst;
	HDC       hdc;
	HBITMAP   hbm;
	HDC       memdc;
	int8_t    resized;
	uint32_t  rw;
	uint32_t  rh;
} MausBackend;

#endif /* MAUS_WIN_H */

