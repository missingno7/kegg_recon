/* stdlib.h - the C library as the 32-bit game world sees it on 64-bit hosts (ILP32 world,
 * docs/android/architecture.md "64-bit: the ILP32 game world"). Only what the historical
 * units call; every function is a Watcom-semantics entry point implemented by
 * port/android/ilp32 (32-bit parameters, low-memory results). */
#ifndef KE_ILP32_STDLIB_H
#define KE_ILP32_STDLIB_H
#include <stddef.h>
void *malloc(size_t size);
void free(void *p);
void exit(int code);
int atexit(void (*fn)(void));
char *getenv(const char *name);
int abs(int value);
char *itoa(int value, char *buffer, int radix);
char *ltoa(long value, char *buffer, int radix);
unsigned long strtoul(const char *s, char **end, int base);
unsigned int _rotl(unsigned int value, int shift);
void _splitpath(const char *path, char *drive, char *dir, char *name, char *ext);
#endif
