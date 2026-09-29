/* string.h - ILP32 game world (see stdlib.h). */
#ifndef KE_ILP32_STRING_H
#define KE_ILP32_STRING_H
#include <stddef.h>
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
char *strcpy(char *d, const char *s);
char *strcat(char *d, const char *s);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strchr(const char *s, int c);
int _stricmp(const char *a, const char *b);
#endif
