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
 *   accesses to the relocated, no-access VGA window (or a legacy VGA effective address) ->
 *                                        the VGA byte API (MOV, MOVZX, LODS/MOVS/STOS,
 *                                        AND/OR/XOR memory forms)
 * Emulated I/O, CLI/STI and INT events are appended to a trace for differential checks.
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

/* A description of visible VGA state. Reading planar memory updates the hardware latches;
 * the helper restores all register data/indexes and leaves the attribute flip-flop in its
 * index phase. DAC components are stored as original 6-bit values.
 */
#define ORACLE_VGA_PLANE_SIZE 0x10000
typedef struct OracleVgaSnapshot {
    uint8_t planes[4][ORACLE_VGA_PLANE_SIZE];
    uint8_t sequencer[8];
    uint8_t graphics_controller[16];
    uint8_t crtc[32];
    uint8_t attribute[32];
    uint8_t dac[256][3];
    uint8_t misc_output;
    uint8_t sequencer_index;
    uint8_t graphics_index;
    uint8_t crtc_index;
    uint8_t attribute_index;
} OracleVgaSnapshot;

int oracle_load(const char *image_path, const char *symbols_path);
void *oracle_object_base(int object);          /* 1..3 */
void *oracle_sym(const char *name);             /* NULL if unknown */
uint32_t oracle_call(void *fn, int argc, const uint32_t *args);
/* Call a host-compiled historical C routine against the same virtual PC. */
uint32_t oracle_port_call(void *fn, int argc, const uint32_t *args);

/* The oracle allocates a no-access host window, relocates original A0000h operands to it,
 * and emulates accesses through the virtual VGA. */
int oracle_vga_window_reserved(void);
/* Translate a canonical A0000h..BFFFFh fixture address to the guarded host alias. */
uint32_t oracle_vga_host_address(uint32_t address);
/* Translate a guarded host alias back to its canonical VGA address. */
uint32_t oracle_vga_guest_address(uint32_t address);
int oracle_vga_snapshot(OracleVgaSnapshot *snapshot);
/* Return 0 when equal; otherwise print the first differing device block and return 1. */
int oracle_vga_snapshot_equal(const OracleVgaSnapshot *a, const OracleVgaSnapshot *b,
                              const char *label);

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
