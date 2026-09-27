/* test_m_0982c.c - compare the translated asset decoder with KE.EXE. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "oracle_test.h"

#define TEST_TAG_COD0 0x30444f43u
#define TEST_TAG_COD1 0x31444f43u

uint32_t decode_and_verify_asset(uint32_t data_address, uint32_t byte_count);
extern uint16_t g_asset_checksum;
extern uint32_t g_asset_trailer_size;
extern uint16_t g_asset_encoding;

static uint32_t rng_state = 0x8f31a24du;
static unsigned reported_mismatches;

/* The decoder's TASM public is at obj1:0982Ch but has no manifest symbol entry. */
static void *original_decoder(void)
{
    return (uint8_t *)oracle_object_base(1) + 0x982cu;
}

static uint32_t next_random(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void store_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static int compare_decode(const char *name, const uint8_t *source, uint32_t size,
                          int mutation)
{
    uint8_t *original = (uint8_t *)malloc(size ? size : 1u);
    uint8_t *translated = (uint8_t *)malloc(size ? size : 1u);
    uint32_t args[2], original_result, translated_result;
    uint16_t original_checksum, original_encoding;
    uint32_t original_trailer_size;
    int different;

    if (!original || !translated) {
        free(original);
        free(translated);
        fprintf(stderr, "    %s: allocation failed\n", name);
        return 1;
    }
    memcpy(original, source, size);
    memcpy(translated, source, size);
    if (mutation >= 0) {
        uint32_t mask = (next_random() & 0xffu) | 1u;
        original[mutation] ^= (uint8_t)mask;
        translated[mutation] ^= (uint8_t)mask;
    }

    args[0] = (uint32_t)(uintptr_t)original;
    args[1] = size;
    original_result = oracle_call(original_decoder(), 2, args);
    memcpy(&original_checksum, oracle_sym("g_asset_checksum"), sizeof original_checksum);
    memcpy(&original_trailer_size, oracle_sym("g_asset_trailer_size"),
           sizeof original_trailer_size);
    memcpy(&original_encoding, oracle_sym("g_asset_encoding"), sizeof original_encoding);

    translated_result = decode_and_verify_asset((uint32_t)(uintptr_t)translated, size);
    different = original_result != translated_result ||
                original_checksum != g_asset_checksum ||
                original_trailer_size != g_asset_trailer_size ||
                original_encoding != g_asset_encoding ||
                memcmp(original, translated, size) != 0;
    if (different && reported_mismatches++ < 10u)
        fprintf(stderr, "    %s%s: original and translated results differ\n", name,
                mutation >= 0 ? " (mutated)" : "");

    free(original);
    free(translated);
    return different;
}

static int read_asset(const char *path, uint8_t **data, uint32_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;
    if (!file)
        return -1;
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    *data = (uint8_t *)malloc(length ? (size_t)length : 1u);
    if (!*data) {
        fclose(file);
        return -1;
    }
    if (fread(*data, 1, (size_t)length, file) != (size_t)length) {
        free(*data);
        fclose(file);
        return -1;
    }
    fclose(file);
    *size = (uint32_t)length;
    return 0;
}

static int test_asset_file(const char *name)
{
    char path[MAX_PATH + 8];
    uint8_t *data = NULL;
    uint32_t size = 0, i;
    int failures = 0;

    snprintf(path, sizeof path, "assets\\%s", name);
    if (read_asset(path, &data, &size) != 0) {
        fprintf(stderr, "    %s: cannot read %s\n", name, path);
        return 1;
    }
    failures += compare_decode(name, data, size, -1);
    for (i = 0; i < 32; i++)
        failures += compare_decode(name, data, size, (int)(next_random() % size));
    free(data);
    return failures;
}

static int test_synthetic_codec(uint32_t tag, const char *codec)
{
    unsigned sample;
    int failures = 0;
    for (sample = 0; sample < 32; sample++) {
        uint32_t size = 14u + next_random() % 2048u;
        uint8_t *data = (uint8_t *)malloc(size);
        char name[64];
        uint32_t i;
        if (!data) {
            fprintf(stderr, "    %s synthetic input allocation failed\n", codec);
            return failures + 1;
        }
        for (i = 0; i < size; i++)
            data[i] = (uint8_t)(next_random() >> 24);
        store_u32(data + size - 4u, tag);
        snprintf(name, sizeof name, "synthetic %s sample %u", codec, sample);
        failures += compare_decode(name, data, size, -1);
        free(data);
    }
    return failures;
}

static int test_all_assets(void)
{
    WIN32_FIND_DATAA entry;
    HANDLE search = FindFirstFileA("assets\\KE_*", &entry);
    int failures = 0, files = 0;

    if (search == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "    cannot enumerate assets\\KE_*\n");
        return 1;
    }
    do {
        if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            failures += test_asset_file(entry.cFileName);
            files++;
        }
    } while (FindNextFileA(search, &entry));
    FindClose(search);
    failures += test_synthetic_codec(TEST_TAG_COD0, "COD0");
    failures += test_synthetic_codec(TEST_TAG_COD1, "COD1");
    if (files == 0) {
        fprintf(stderr, "    no assets\\KE_* files found\n");
        failures++;
    }
    printf("    compared %d KE_* assets with 32 randomized corruptions each, plus 64 COD0/COD1 inputs\n",
           files);
    if (failures)
        printf("    %d comparisons differed (%u mismatch details shown)\n", failures,
               reported_mismatches < 10u ? reported_mismatches : 10u);
    return failures;
}

void register_m_0982c_tests(void)
{
    oracle_register("m_0982c asset decoder (assets + randomized corruptions)", test_all_assets);
}
