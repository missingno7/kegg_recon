/* G2 regression: the racket checksum spans the original CONST literal bytes. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "oracle.h"
#include "oracle_test.h"

extern unsigned char *best_of_the_bests_text;
extern unsigned char *immortality_cheat_phrase;

static uint32_t span_xor(const unsigned char *bytes, size_t length)
{
    uint32_t value = 0;
    size_t offset;
    if (length & 3)
        return 0xffffffffu;
    for (offset = 0; offset < length; offset += sizeof value) {
        uint32_t word;
        memcpy(&word, bytes + offset, sizeof word);
        value ^= word;
    }
    return value;
}

static int test_original_const_span(void)
{
    uintptr_t original_begin = *(uint32_t *)oracle_sym("best_of_the_bests_text");
    uintptr_t original_end = *(uint32_t *)oracle_sym("immortality_cheat_phrase");
    const unsigned char *port_begin = best_of_the_bests_text;
    uintptr_t port_end = (uintptr_t)immortality_cheat_phrase;
    size_t original_length, port_length;
    uint32_t original_xor, port_xor;

    if (original_end < original_begin || port_end < (uintptr_t)port_begin) {
        printf("    invalid anti-cheat CONST span bounds\n");
        return 1;
    }
    original_length = (size_t)(original_end - original_begin);
    port_length = (size_t)(port_end - (uintptr_t)port_begin);
    if (original_length != port_length) {
        printf("    anti-cheat CONST span lengths differ: original=%lu port=%lu\n",
               (unsigned long)original_length, (unsigned long)port_length);
        return 1;
    }
    original_xor = span_xor((const unsigned char *)original_begin, original_length);
    port_xor = span_xor(port_begin, port_length);
    if (memcmp(port_begin, (const void *)original_begin, original_length) != 0 ||
        original_xor != 0 || port_xor != 0) {
        printf("    anti-cheat CONST span mismatch: bytes=%s xor original/port=%08lX/%08lX\n",
               memcmp(port_begin, (const void *)original_begin, original_length) ? "different" : "equal",
               (unsigned long)original_xor, (unsigned long)port_xor);
        return 1;
    }
    printf("    anti-cheat CONST span byte-equal (%lu bytes), XOR=0\n",
           (unsigned long)original_length);
    return 0;
}

void register_g2_tests(void)
{
    oracle_register("G2 original anti-cheat CONST span bytes", test_original_const_span);
}
