#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

#define CLAMP(n, min, max) ((n) < (min) ? (min) : (n) > (max) ? (max) : (n))

/* perror `msg` and exit program with `ec`
   TODO? stdarg */
void die(int ec, const char* msg, ...);

/* pack r, g, b and a into u32 (AARRGGBB) */
uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

#endif /* UTILS_H */

