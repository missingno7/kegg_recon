/* oracle.h - run original KE.EXE machine code natively inside a 32-bit Windows process.
 *
 * oracle_load() maps the three LE objects of KE.EXE (exported by port/tools/le_export.py)
 * at host-chosen addresses, zero-fills BSS and applies every fixup (off32: target object
 * base + offset; sel16: the process's flat DS). Original routines are then called with the
 * Watcom -3s convention, which for int/pointer arguments is cdecl: arguments pushed right
 * to left, caller pops, result in EAX, EBX/ESI/EDI/EBP preserved, DF clear.
 *
 * Instructions that fault in user mode are emulated by a vectored exception handler:
 *   IN/OUT (E4-E7, EC-EF, 66h prefix)   -> oracle port hooks (default: the virtual PC)
 *   CLI/STI                             -> recorded (virtual IF)
 *   INT nn (CD nn)                      -> vhw_int() with the register image
 *   MOV Sreg,r16 (8E /r, mod=11)        -> ignored (every selector is flat)
 * Every emulated event is appended to a trace so a test can compare the I/O sequence of
 * the original with that of the translation. Accesses to the VGA window (A0000h) are NOT
 * emulated yet (work package "oracle-vga"): routines that write video memory directly can
 * only be compared once that hook exists.
 */
#ifndef KE_ORACLE_H
#define KE_ORACLE_H
#include <stdint.h>

typedef struct OracleEvent {
    uint8_t kind;          /* 'I' in, 'O' out, 'C' cli, 'S' sti, 'N' int */
    uint8_t size;          /* 1, 2, 4 */
    uint16_t port;         /* port or interrupt number */
    uint32_t value;
} OracleEvent;

int oracle_load(const char *image_path, const char *symbols_path);
void *oracle_object_base(int object);          /* 1..3 */
void *oracle_sym(const char *name);             /* NULL if unknown */
uint32_t oracle_call(void *fn, int argc, const uint32_t *args);

/* I/O hooks used while original code runs (default: vhw_port_in / vhw_port_out). */
typedef uint32_t (*oracle_in_fn)(uint16_t port, int size);
typedef void (*oracle_out_fn)(uint16_t port, uint32_t value, int size);
void oracle_set_port_hooks(oracle_in_fn in, oracle_out_fn out);

/* Trace of emulated privileged instructions since the last reset. */
void oracle_trace_reset(void);
int oracle_trace_count(void);
const OracleEvent *oracle_trace(void);
/* Record an event from translated code (port side) into the same format. */
void oracle_trace_add(uint8_t kind, uint16_t port, uint32_t value, int size);

#endif
