#if !defined(BACKEND_WIN)
#define _POSIX_C_SOURCE 199309L
#else
#include <windows.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "maus.h"

static void vlog(FILE* fd, const char* fmt, va_list ap);

static void vlog(FILE* fd, const char* fmt, va_list ap)
{
	fprintf(fd, "maus: ");
	vfprintf(fd, fmt, ap);
}

void maus_die(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vlog(stderr, fmt, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}

uint64_t maus_get_time_ns(void)
{
#if !defined(_WIN32)
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000 + ts.tv_nsec;
#else
	static LARGE_INTEGER frequency = {0};
	LARGE_INTEGER counter;
	uint64_t sec;
	uint64_t rem;

	if (frequency.QuadPart == 0)
		QueryPerformanceFrequency(&frequency);

	QueryPerformanceCounter(&counter);

	sec = counter.QuadPart / frequency.QuadPart;
	rem = counter.QuadPart % frequency.QuadPart;

	return (sec * 1000000000) + ((rem * 1000000000) / frequency.QuadPart);
#endif
}

void maus_log(FILE* fd, const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vlog(fd, fmt, ap);
	fputc('\n', fd);
	va_end(ap);
}

void maus_target_fps(Maus* mw, uint32_t fps)
{
	uint64_t ns_frame;
	uint64_t cur_time;
	uint64_t elapsed;
	uint64_t sleep;

	#if !defined(_WIN32)
	struct timespec ts;
	#endif

	if (fps == 0)
		return;

	ns_frame = 1000000000 / fps; /* max ns a frame can take */
	cur_time = maus_get_time_ns();
	elapsed = cur_time - mw->frame_time_last;

	/* if early finish, sleep for the remaining time balance */
	if (elapsed < ns_frame) {
		sleep = ns_frame - elapsed;
	#if !defined(_WIN32)
		ts.tv_sec = sleep / 1000000000;
		ts.tv_nsec = sleep % 1000000000;
		nanosleep(&ts, NULL);
	#else
		Sleep((DWORD)((sleep + 999999) / 1000000));
	#endif
	}

	mw->frame_time_last = maus_get_time_ns();
}

