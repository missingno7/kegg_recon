/* m_0982c_0995c.c - instruction-order translation of the asset trailer decoder.
 *
 * The assembly decodes complete dwords in place, then XORs the decoded words into its
 * checksum accumulator. Its use of the final dword as the codec tag and the distinct COD0,
 * COD1, and COD2 trailer rules are kept literally here for oracle comparison.
 */
#include <stdint.h>

#define ASSET_TAG_COD0_ID        0x30444f43u
#define ASSET_TAG_COD1_ID        0x31444f43u
#define ASSET_TAG_COD2_ID        0x32444f43u
#define ASSET_CODEC_COD0         1u
#define ASSET_CODEC_COD2         2u
#define ASSET_COD0_TRAILER_BYTES 10u
#define ASSET_COD2_TRAILER_BYTES 8u
#define ASSET_DECODE_LIMIT       0x400u
#define ASSET_CHECKSUM_SEED      0x1234u

uint16_t g_asset_checksum = 0;
uint32_t g_asset_trailer_size = 0;
uint16_t g_asset_encoding = 0;

static uint16_t load_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t load_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void store_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint16_t ror16(uint16_t value, unsigned count)
{
    count &= 15u;
    return (uint16_t)((value >> count) | (value << ((16u - count) & 15u)));
}

static uint16_t rol16(uint16_t value, unsigned count)
{
    count &= 15u;
    return (uint16_t)((value << count) | (value >> ((16u - count) & 15u)));
}

static uint32_t ror32(uint32_t value, unsigned count)
{
    count &= 31u;
    return (value >> count) | (value << ((32u - count) & 31u));
}

static uint32_t rol32(uint32_t value, unsigned count)
{
    count &= 31u;
    return (value << count) | (value >> ((32u - count) & 31u));
}

static void decode_words(uint8_t *payload, uint32_t count, uint32_t *key,
                         uint32_t *checksum)
{
    uint32_t i;
    for (i = 0; i < count; i++) {            /* lodsd / xor / stosd / xor / rol / loop */
        uint32_t plain = load_u32(payload + i * 4u) ^ *key;
        store_u32(payload + i * 4u, plain);
        *checksum ^= plain;
        *key = rol32(*key, 1);
    }
}

uint32_t decode_and_verify_asset(uint32_t data_address, uint32_t byte_count)
{
    uint8_t *data = (uint8_t *)(uintptr_t)data_address;
    uint32_t tag, payload_bytes, word_count, key, accumulator;
    uint8_t *trailer;

    g_asset_checksum = 0;
    g_asset_trailer_size = 0;
    g_asset_encoding = 0xffffu;
    tag = load_u32(data + byte_count - 4u);

    if (tag == ASSET_TAG_COD2_ID) {
        g_asset_encoding = ASSET_CODEC_COD2;
        g_asset_trailer_size = ASSET_COD2_TRAILER_BYTES;
        payload_bytes = byte_count - ASSET_COD2_TRAILER_BYTES;
        trailer = data + payload_bytes;
        g_asset_checksum = load_u16(trailer);

        /* The original builds EBX from two rotations of the same trailer word. */
        key = ((uint32_t)ror16(load_u16(trailer + 2), 7) << 16) |
              (uint32_t)rol16(load_u16(trailer + 2), 3);
        if ((int32_t)payload_bytes > (int32_t)ASSET_DECODE_LIMIT)
            payload_bytes = ASSET_DECODE_LIMIT;
        word_count = payload_bytes >> 2;
        accumulator = 0;
        decode_words(data, word_count, &key, &accumulator);
    } else if (tag == ASSET_TAG_COD0_ID || tag == ASSET_TAG_COD1_ID) {
        g_asset_encoding = ASSET_CODEC_COD0;
        g_asset_trailer_size = ASSET_COD0_TRAILER_BYTES;
        payload_bytes = byte_count - ASSET_COD0_TRAILER_BYTES;
        trailer = data + payload_bytes;
        g_asset_checksum = load_u16(trailer);
        key = ror32(load_u32(trailer + 2), 7);
        word_count = payload_bytes >> 2;
        accumulator = ASSET_CHECKSUM_SEED;
        decode_words(data, word_count, &key, &accumulator);
    } else {
        return 0xffffffffu;
    }

    g_asset_checksum ^= (uint16_t)accumulator;
    g_asset_checksum ^= (uint16_t)(accumulator >> 16);
    return g_asset_checksum == 0 ? g_asset_trailer_size : 0xffffffffu;
}
