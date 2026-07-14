#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

/* perror `msg` and exit program with `ec`
   TODO? stdarg */
void die(int ec, const char* msg);

/* pack r, g, b and a into u32 (AARRGGBB) */
uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* malloc, die on failure */
void* xmalloc(size_t len);

#endif /* UTILS_H */
