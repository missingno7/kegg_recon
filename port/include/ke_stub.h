/* ke_stub.h - marking of not-yet-ported code.
 *
 * KE_ASM_STUB(name, module): stands in for a PUBLIC code label of a historical TASM module
 * until port/asm/<module>.c translates it. Logs the first call and returns 0.
 * KE_STUB(ret, name, params, value): the same for any other unported function.
 *
 * Every stub is greppable: `grep -rn "KE_.*STUB(" port/`.
 */
#ifndef KE_STUB_H
#define KE_STUB_H
#include "ke_port.h"

#define KE_ASM_STUB(name, module)                                                          \
    int name(void);                                                                        \
    int name(void) { ke_stub_hit(#name, "asm/" module ".asm"); return 0; }

#define KE_STUB(ret, name, params, value)                                                  \
    ret name params { ke_stub_hit(#name, __FILE__); return value; }

#endif
