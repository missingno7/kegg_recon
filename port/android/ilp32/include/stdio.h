/* stdio.h - ILP32 game world (see stdlib.h). FILE handles are low-memory slots. */
#ifndef KE_ILP32_STDIO_H
#define KE_ILP32_STDIO_H
#include <stddef.h>
typedef struct KeFile32 FILE;
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define EOF (-1)
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
size_t fread(void *p, size_t size, size_t n, FILE *f);
size_t fwrite(const void *p, size_t size, size_t n, FILE *f);
int fseek(FILE *f, long offset, int whence);
long ftell(FILE *f);
int printf(const char *format, ...);
int fflush(FILE *f);
#endif
