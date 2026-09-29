/* stddef.h - ILP32 game world: Watcom 386 widths on every compiler that builds it (the
 * arm64_32 front end would otherwise make size_t `unsigned long`, which a historical unit
 * redefines as `unsigned int`). */
#ifndef KE_ILP32_STDDEF_H
#define KE_ILP32_STDDEF_H
typedef unsigned int size_t;
typedef int ptrdiff_t;
#ifndef NULL
#define NULL ((void *)0)
#endif
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif
