#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief exit program after printing a message
 *
 * @param ec error code
 * @param msg message to print
 */
void die(int ec, const char* msg);

/**
 * @brief convert red, green, blue, alpha values to a packed value
 *
 * @param r red value
 * @param g green value
 * @param b blue value
 * @param a alpha value
 *
 * @return packed rbga value
 */
uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/**
 * @brief allocate or die
 *
 * @param len bytes to allocate
 */
void* xmalloc(size_t len);

#endif /* UTIL_H */

