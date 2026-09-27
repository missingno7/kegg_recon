/* PCX decoder called by the picture-file loader. */
#define PCX_HEADER_BYTES 0x80
#define PCX_HEADER_SIGNATURE 0x0801050a
#define PCX_RLE_MARKER 0xc0
#define PCX_RLE_COUNT_MASK 0x3f
#define PCX_PALETTE_BYTES 0x300
#define PCX_PALETTE_ENTRIES 0x100
#define PCX_IMAGE_FORMAT_INDEXED 4
#define PCX_INVALID_HEADER_ERROR 0x0a01

int pcx_allocation_size;
/* These two zero-filled words share this unit's data but their callers are outside the recovered use path. */
int g_e1c8;
int g_e1cc;
int pcx_pixel_and_palette_payload_bytes;

/* PCX's 128-byte header followed by RLE pixels and the 256-color palette. */
struct PCXHeader {
    unsigned int signature;
    unsigned char reserved_4_to_11[8];
    short pixel_width;
    short pixel_height;
};

struct DecodedImage {
    unsigned char *pixels;
    unsigned char *palette;
    int width;
    int height;
    int pixel_format;
    int palette_entries;
    int allocation_size;
    int palette_size;
    int pixel_size;
};

int decode_pcx_image(int source_address, int pixel_buffer_address, struct DecodedImage *image)
{
    unsigned char run_value;
    struct PCXHeader *header;
    int source_cursor;
    unsigned int output_cursor;
    int encoded_byte;
    int width;
    int height;
    int pixel_count;
    int result;

    header = (struct PCXHeader *)source_address;
    source_cursor = source_address + PCX_HEADER_BYTES;
    output_cursor = pixel_buffer_address;
    if (header->signature != PCX_HEADER_SIGNATURE) {
        result = PCX_INVALID_HEADER_ERROR;
        return result;
    }
    width = header->pixel_width;
    height = header->pixel_height;
    pixel_count = width * height;
    while ((unsigned)(pixel_buffer_address + pixel_count) > output_cursor) {
        encoded_byte = *(unsigned char *)(source_cursor++);
        if ((encoded_byte & PCX_RLE_MARKER) == PCX_RLE_MARKER) {
            run_value = *(unsigned char *)(source_cursor++);
            for (encoded_byte &= PCX_RLE_COUNT_MASK; encoded_byte > 0; encoded_byte--) {
                *(unsigned char *)(output_cursor++) = run_value;
            }
        } else {
            *(unsigned char *)(output_cursor++) = (signed char)encoded_byte;
        }
    }
    /* Skip the one-byte palette marker after the RLE stream. */
    source_cursor++;
    output_cursor = pixel_buffer_address + pixel_count;
    for (encoded_byte = 0; encoded_byte < PCX_PALETTE_BYTES; encoded_byte++) {
        *(unsigned char *)(output_cursor++) =
        (unsigned char)(*(unsigned char *)(source_cursor++) >> 2);
    }
    pcx_allocation_size = pixel_count + PCX_HEADER_BYTES + PCX_PALETTE_BYTES;
    pcx_pixel_and_palette_payload_bytes = pixel_count + PCX_PALETTE_BYTES;
    image->pixels = (unsigned char *)pixel_buffer_address;
    image->width = width;
    image->height = height;
    image->palette_entries = PCX_PALETTE_ENTRIES;
    image->pixel_format = PCX_IMAGE_FORMAT_INDEXED;
    image->palette_size = image->palette_entries * 3;
    image->pixel_size = image->width * image->height;
    image->allocation_size = image->palette_size + image->pixel_size;
    image->palette = image->pixels + image->pixel_size;
    result = 0;
    return result;
}
