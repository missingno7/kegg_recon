/* G2 regression: the racket animation's anti-cheat checksum spans two linked strings. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "oracle_test.h"

typedef struct TestRacket {
    int fields[30];
    unsigned char *sprite_pointer;
} TestRacket;
typedef struct TestPlayerFlags {
    unsigned char control_flags, spell_flags, reserved_2, reserved_3;
} TestPlayerFlags;
typedef struct TestResult { int balls, error; } TestResult;

extern TestRacket *racket_object;
extern TestPlayerFlags *player_key_flags;
extern int current_ball_count;
extern int file_error_state;
extern unsigned char *best_of_the_bests_text;
extern unsigned char *immortality_cheat_phrase;
extern unsigned char *image_buffer_cursor;
void update_racket_state(void);

static uint32_t span_checksum(uintptr_t begin, uintptr_t end)
{
    uint32_t checksum;
    if (end < begin || end - begin > 0x100000u)
        return 0xffffffffu;
    checksum = *(const uint32_t *)begin;
    begin += 4;
    while (begin < end) {
        checksum ^= *(const uint32_t *)begin;
        begin += 4;
    }
    return checksum;
}

static TestResult run_racket_update(int original)
{
    TestRacket racket;
    TestPlayerFlags flags;
    unsigned char commands[128] = {0};
    uint32_t args[1] = {0};
    TestResult result;
    memset(&racket, 0, sizeof racket);
    memset(&flags, 0, sizeof flags);
    flags.control_flags = 2 | 4; /* move animation, with sprite drawing suppressed */
    racket.fields[0x50 / 4] = 1;
    racket.fields[0x54 / 4] = 8;
    if (original) {
        *(TestRacket **)oracle_sym("racket_object") = &racket;
        *(TestPlayerFlags **)oracle_sym("player_key_flags") = &flags;
        *(int *)oracle_sym("current_ball_count") = 0;
        *(int *)oracle_sym("file_error_state") = 1234;
        *(unsigned char **)oracle_sym("image_buffer_cursor") = commands;
        oracle_call(oracle_sym("update_racket_state"), 0, args);
        result.balls = *(int *)oracle_sym("current_ball_count");
        result.error = *(int *)oracle_sym("file_error_state");
    } else {
        racket_object = &racket;
        player_key_flags = &flags;
        current_ball_count = 0;
        file_error_state = 1234;
        image_buffer_cursor = commands;
        oracle_port_call((void *)update_racket_state, 0, args);
        result.balls = current_ball_count;
        result.error = file_error_state;
    }
    return result;
}

static int make_byte_writable(uintptr_t address, DWORD *old_protect)
{
    return VirtualProtect((void *)address, 1, PAGE_EXECUTE_READWRITE, old_protect) ? 0 : 1;
}

static int restore_byte_protection(uintptr_t address, DWORD old_protect)
{
    DWORD ignored;
    return VirtualProtect((void *)address, 1, old_protect, &ignored) ? 0 : 1;
}

static int test_racket_checksum_differential(void)
{
    uintptr_t original_best, original_end, ported_best, ported_end;
    uint32_t original_racket_before, original_flags_before, original_cursor_before;
    uint32_t ported_racket_before, ported_flags_before, ported_cursor_before;
    int original_balls_before, original_error_before, ported_balls_before, ported_error_before;
    DWORD original_protect = 0, ported_protect = 0;
    uint8_t original_byte = 0, ported_byte = 0;
    TestResult original, ported, original_mutated, ported_mutated;
    int failed = 0, original_writable = 0, ported_writable = 0;

    original_best = *(uint32_t *)oracle_sym("best_of_the_bests_text");
    original_end = *(uint32_t *)oracle_sym("immortality_cheat_phrase");
    ported_best = (uintptr_t)best_of_the_bests_text;
    ported_end = (uintptr_t)immortality_cheat_phrase;
    printf("    G2 checksum XOR: original=%08lX port=%08lX (expected port baseline 18355D5B)\n",
           (unsigned long)span_checksum(original_best, original_end),
           (unsigned long)span_checksum(ported_best, ported_end));

    original_racket_before = *(uint32_t *)oracle_sym("racket_object");
    original_flags_before = *(uint32_t *)oracle_sym("player_key_flags");
    original_cursor_before = *(uint32_t *)oracle_sym("image_buffer_cursor");
    original_balls_before = *(int *)oracle_sym("current_ball_count");
    original_error_before = *(int *)oracle_sym("file_error_state");
    ported_racket_before = (uint32_t)(uintptr_t)racket_object;
    ported_flags_before = (uint32_t)(uintptr_t)player_key_flags;
    ported_cursor_before = (uint32_t)(uintptr_t)image_buffer_cursor;
    ported_balls_before = current_ball_count;
    ported_error_before = file_error_state;

    original = run_racket_update(1);
    ported = run_racket_update(0);
    if (original.balls != 0 || original.error != 1234 ||
        ported.balls != original.balls || ported.error != original.error) {
        printf("    clean string span side effects: original balls/error %d/%d, port %d/%d\n",
               original.balls, original.error, ported.balls, ported.error);
        failed++;
    }

    original_writable = make_byte_writable(original_best + 8, &original_protect) == 0;
    ported_writable = make_byte_writable(ported_best + 8, &ported_protect) == 0;
    if (!original_writable || !ported_writable) {
        printf("    could not make checksum string bytes writable for mutation check\n");
        failed++;
        goto restore_globals;
    }
    original_byte = *(uint8_t *)(original_best + 8);
    ported_byte = *(uint8_t *)(ported_best + 8);
    *(volatile uint8_t *)(original_best + 8) = (uint8_t)(original_byte ^ 1u);
    *(volatile uint8_t *)(ported_best + 8) = (uint8_t)(ported_byte ^ 1u);
    original_mutated = run_racket_update(1);
    ported_mutated = run_racket_update(0);
    *(volatile uint8_t *)(original_best + 8) = original_byte;
    *(volatile uint8_t *)(ported_best + 8) = ported_byte;
    if (restore_byte_protection(original_best + 8, original_protect) ||
        restore_byte_protection(ported_best + 8, ported_protect)) {
        printf("    could not restore checksum string page protection\n");
        failed++;
    }
    if (original_mutated.balls != 5000 || original_mutated.error != 0 ||
        ported_mutated.balls != original_mutated.balls ||
        ported_mutated.error != original_mutated.error) {
        printf("    mutated string span side effects: original balls/error %d/%d, port %d/%d\n",
               original_mutated.balls, original_mutated.error,
               ported_mutated.balls, ported_mutated.error);
        failed++;
    }

restore_globals:
    if (original_writable)
        restore_byte_protection(original_best + 8, original_protect);
    if (ported_writable)
        restore_byte_protection(ported_best + 8, ported_protect);
    *(uint32_t *)oracle_sym("racket_object") = original_racket_before;
    *(uint32_t *)oracle_sym("player_key_flags") = original_flags_before;
    *(uint32_t *)oracle_sym("image_buffer_cursor") = original_cursor_before;
    *(int *)oracle_sym("current_ball_count") = original_balls_before;
    *(int *)oracle_sym("file_error_state") = original_error_before;
    racket_object = (TestRacket *)(uintptr_t)ported_racket_before;
    player_key_flags = (TestPlayerFlags *)(uintptr_t)ported_flags_before;
    image_buffer_cursor = (unsigned char *)(uintptr_t)ported_cursor_before;
    current_ball_count = ported_balls_before;
    file_error_state = ported_error_before;
    return failed;
}

void register_g2_tests(void)
{
    oracle_register("G2 racket animation checksum against original and mutation", test_racket_checksum_differential);
}
