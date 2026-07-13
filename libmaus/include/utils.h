#ifndef UTIL_H
#define UTIL_H

/* duplicate a string. returns pointer to address of
   newly allocated string buffer. if NULL, allocation
   failed or src is NULL
   note: you must free this newly allocated string */
char* strdup(const char* src);

#endif /* UTIL_H */

