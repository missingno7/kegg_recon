/* m_0a284_0a51f.c - instruction-order translation of asm/m_0a284_0a51f.asm.
 *
 * The public C interface for find_iff_chunk adapts the original internal register interface:
 * pass the canonical big-endian chunk ID and receive the address of its size word, or NULL.
 * In the TASM routine, the same address is returned in EBX and absence is reported in CF.
 *
 * The decoder retains the original's format quirks: raw bodies copy whole dwords only,
 * ByteRun1 runs may cross the output boundary before the overrun error is reported, and ILBM
 * conversion processes complete 16-pixel groups before copying the file prefix back.
 */
#include <stdint.h>

/* EQUs from the module. Chunk IDs passed to find_iff_chunk are canonical big-endian values;
 * FORM is also compared directly against the little-endian dword loaded from its file bytes. */
#define IFF_FORM_ID 0x4D524F46u
#define IFF_BODY_ID 0x424F4459u
#define IFF_BMHD_ID 0x424D4844u
#define IFF_CMAP_ID 0x434D4150u
#define IFF_ILBM_ID 0x4D424C49u
#define IFF_FORM_TYPE_OFFSET 8u
#define IFF_BITMAP_HEADER_WIDTH 4u
#define IFF_BITMAP_HEADER_HEIGHT 6u
#define IFF_BITMAP_HEADER_COMPRESSION 0x0Eu
#define IFF_ERROR_CLASS 8u
#define IFF_ERROR_INVALID_FORM 1u
#define IFF_ERROR_BODY_MISSING 2u
#define IFF_ERROR_HEADER_MISSING 3u
#define IFF_ERROR_PALETTE_MISSING 4u
#define IFF_ERROR_BODY_OVERRUN 5u
#define BYTERUN1_NOOP_CONTROL 0x80u
#define IFF_COMPRESSION_NONE 0u

/* Public _DATA labels, in the order and sizes used by the original module. */
uint32_t g_iff_error = 0;
uint32_t g_iff_file = 0;
uint32_t g_iff_pixels = 0;
uint32_t g_iff_pixel_count = 0;
uint32_t g_iff_palette_bytes = 0;
uint32_t g_iff_decoder_workspace[5] = {0, 0, 0, 0, 0};

extern int iff_width_pixels;
extern int iff_height_pixels;
extern int iff_output_byte_count;
extern int iff_decoded_pixel_count;

typedef struct IffImageLayout {
    uint32_t pixel_buffer;
    uint32_t palette_buffer;
    uint32_t width_pixels;
    uint32_t height_pixels;
    uint32_t format_code;
    uint32_t palette_entries;
    uint32_t file_byte_count;
    uint32_t palette_byte_count;
    uint32_t pixel_count;
} IffImageLayout;

static uint32_t read_le16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t read_be16(const uint8_t *p)
{
    return ((uint32_t)p[0] << 8) | (uint32_t)p[1];
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Equivalent to find_iff_chunk's do/while scan, including its four-byte FORM-end offset. */
uint32_t find_iff_chunk(uint32_t chunk_id)
{
    uint8_t *file = (uint8_t *)(uintptr_t)g_iff_file;
    uint32_t base = (uint32_t)(uintptr_t)file;
    uint32_t cursor = base + 0x0Cu;
    uint32_t end = cursor + read_be32(file + 4);
    do {
        uint8_t *chunk = (uint8_t *)(uintptr_t)cursor;
        uint32_t id = read_be32(chunk);
        uint32_t size;
        if (id == chunk_id)
            return cursor + 4u;              /* EBX points at the chunk size word. */
        size = read_be32(chunk + 4);
        cursor += 8u;
        cursor += size;
        cursor++;
        cursor &= ~1u;                       /* and esi,-2 aligns the absolute address. */
    } while ((int32_t)cursor < (int32_t)end);
    return 0;
}

static void set_iff_error(uint32_t code)
{
    g_iff_error = (IFF_ERROR_CLASS << 8) | (code & 0xFFu);
}

/* The original REP MOVSD performs one read of four bytes before each four-byte write. */
static void copy_dwords(uint8_t **source, uint8_t **destination, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        uint8_t word[4];
        word[0] = (*source)[0];
        word[1] = (*source)[1];
        word[2] = (*source)[2];
        word[3] = (*source)[3];
        (*destination)[0] = word[0];
        (*destination)[1] = word[1];
        (*destination)[2] = word[2];
        (*destination)[3] = word[3];
        *source += 4;
        *destination += 4;
    }
}

static void copy_bytes(uint8_t **source, uint8_t **destination, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        uint8_t value = **source;
        **destination = value;
        (*source)++;
        (*destination)++;
    }
}

static int decode_byterun1(uint8_t **source, uint8_t **destination, uint32_t limit)
{
    for (;;) {
        uint8_t control = *(*source)++;
        uint32_t count;
        if (control < BYTERUN1_NOOP_CONTROL) {
            count = (uint32_t)control + 1u;
            copy_bytes(source, destination, count);
        } else if (control > BYTERUN1_NOOP_CONTROL) {
            uint8_t value;
            count = (uint32_t)(uint8_t)(0u - control) + 1u;
            value = *(*source)++;
            while (count--)
                *(*destination)++ = value;
        } else {
            continue;                         /* QUIRK: 80h consumes no data or output. */
        }
        if ((int32_t)(uintptr_t)*destination < (int32_t)limit)
            continue;
        if ((uint32_t)(uintptr_t)*destination != limit) {
            set_iff_error(IFF_ERROR_BODY_OVERRUN);
            return 1;
        }
        return 0;
    }
}

/* Translate the eight plane-bit tests for each output pixel in their original order. */
static void convert_ilbm(uint8_t *pixels, uint8_t *file)
{
    uint8_t *esi = pixels;
    uint8_t *edi = file;
    uint32_t row;
    uint32_t width = (uint32_t)iff_width_pixels;
    uint32_t height = (uint32_t)iff_height_pixels;

    for (row = 0; row < height; row++) {
        uint32_t groups = width >> 4;
        uint32_t group;
        for (group = 0; group < groups; group++) {
            uint32_t block;
            for (block = 0; block < 2; block++) {
                uint32_t pixel;
                for (pixel = 0; pixel < 8; pixel++) {
                    int bit = 7 - (int)pixel;
                    uint32_t plane;
                    uint8_t color = 0;
                    uint32_t plane_step = width >> 3;
                    esi += width;
                    esi -= plane_step;
                    for (plane = 0; plane < 8; plane++) {
                        uint16_t bits;
                        bits = (uint16_t)read_le16(esi - plane * plane_step);
                        color = (uint8_t)(color + color + ((bits >> bit) & 1u));
                    }
                    esi -= 7u * plane_step;
                    *edi++ = color;
                }
                esi++;
            }
        }
        esi += width - (width >> 3);
    }
}

uint32_t decode_iff_ilbm_image(uint32_t file_address, uint32_t pixel_address,
                               IffImageLayout *image)
{
    uint8_t *file = (uint8_t *)(uintptr_t)file_address;
    uint8_t *pixels = (uint8_t *)(uintptr_t)pixel_address;
    uint8_t *body;
    uint8_t *header;
    uint8_t *palette;
    uint8_t *source;
    uint8_t *destination;
    uint32_t pixel_count;
    uint32_t palette_bytes;
    uint32_t i;
    uint32_t output_count;

    g_iff_file = file_address;
    g_iff_pixels = pixel_address;
    destination = pixels;
    if (read_le32(file) != IFF_FORM_ID) {
        set_iff_error(IFF_ERROR_INVALID_FORM);
        goto store_result;
    }

    body = (uint8_t *)(uintptr_t)find_iff_chunk(IFF_BODY_ID);
    if (!body) {
        set_iff_error(IFF_ERROR_BODY_MISSING);
        goto store_result;
    }
    source = body + 4;
    destination = pixels;
    header = (uint8_t *)(uintptr_t)find_iff_chunk(IFF_BMHD_ID);
    if (!header) {
        set_iff_error(IFF_ERROR_HEADER_MISSING);
        goto store_result;
    }

    iff_width_pixels = (int)read_be16(header + IFF_BITMAP_HEADER_WIDTH);
    iff_height_pixels = (int)read_be16(header + IFF_BITMAP_HEADER_HEIGHT);
    pixel_count = (uint32_t)iff_width_pixels * (uint32_t)iff_height_pixels;
    g_iff_pixel_count = pixel_count;
    g_iff_decoder_workspace[0] = pixel_address + pixel_count;
    if (header[IFF_BITMAP_HEADER_COMPRESSION] == IFF_COMPRESSION_NONE) {
        copy_dwords(&source, &destination, pixel_count >> 2);
    } else {
        if (decode_byterun1(&source, &destination, g_iff_decoder_workspace[0]))
            goto store_result;
    }

    palette = (uint8_t *)(uintptr_t)find_iff_chunk(IFF_CMAP_ID);
    if (!palette) {
        set_iff_error(IFF_ERROR_PALETTE_MISSING);
        goto store_result;
    }
    palette_bytes = read_be32(palette);
    g_iff_palette_bytes = palette_bytes;
    source = palette + 4;
    for (i = 0; i < palette_bytes; i++)
        *destination++ = (uint8_t)(*source++ >> 2);
    output_count = (uint32_t)(uintptr_t)destination - pixel_address;

    if (read_le32(file + IFF_FORM_TYPE_OFFSET) != IFF_ILBM_ID)
        goto store_result_with_count;

    convert_ilbm(pixels, file);
    pixel_count = (uint32_t)iff_width_pixels * (uint32_t)iff_height_pixels;
    /* QUIRK: STD REP MOVSB walks both ranges backwards, including for overlapping buffers. */
    for (i = pixel_count; i != 0; i--)
        pixels[i - 1] = file[i - 1];
    output_count = pixel_count + g_iff_palette_bytes;

store_result_with_count:
    iff_output_byte_count = (int)output_count;
    goto store_descriptor;

store_result:
    /* On invalid FORM / missing BODY the assembly stores its incoming EDI in this global.
     * That caller-register leak is not a C-level input; keep the stable destination value. */
    iff_output_byte_count = (int)(uint32_t)(uintptr_t)destination;

store_descriptor:
    iff_decoded_pixel_count = (int)g_iff_pixel_count;
    image->pixel_buffer = pixel_address;
    pixel_count = (uint32_t)iff_width_pixels * (uint32_t)iff_height_pixels;
    image->pixel_count = pixel_count;
    image->palette_buffer = pixel_address + pixel_count;
    image->width_pixels = (uint32_t)iff_width_pixels;
    image->height_pixels = (uint32_t)iff_height_pixels;
    image->palette_entries = 0x100u;
    image->palette_byte_count = 0x300u;
    image->file_byte_count = image->palette_byte_count + image->pixel_count;
    image->format_code = 1;
    return g_iff_error;
}
