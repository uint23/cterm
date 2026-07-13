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
	return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#else
	static LARGE_INTEGER frequency = {0};
	if (frequency.QuadPart == 0)
		QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);

	uint64_t sec = counter.QuadPart / frequency.QuadPart;
	uint64_t rem = counter.QuadPart % frequency.QuadPart;

	return (sec * 1000000000ULL) + ((rem * 1000000000ULL) / frequency.QuadPart);
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
	if (fps == 0)
		return;

	uint64_t ns_frame = 1000000000ULL / fps; /* max ns a frame can take */
	uint64_t cur_time = maus_get_time_ns();
	uint64_t elapsed = cur_time - mw->frame_time_last;

	/* if early finish, sleep for the remaining time balance */
	if (elapsed < ns_frame) {
		uint64_t sleep = ns_frame - elapsed;
	#if !defined(_WIN32)
		struct timespec ts;
		ts.tv_sec = sleep / 1000000000ULL;
		ts.tv_nsec = sleep % 1000000000ULL;
		nanosleep(&ts, NULL);
	#else /* unix */
		Sleep((DWORD)((sleep + 999999ULL) / 1000000ULL));
	#endif /* platform */
	}

	mw->frame_time_last = maus_get_time_ns();
}

