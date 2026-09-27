#define decode_pcx_image f_a0e0 /* Keep T08's legacy import without changing its bytes. */
int g_pcx_allocation_size;
int g_e1c8;
int g_e1cc;
int pcx_pixel_and_palette_payload_bytes;

/* PCX image reader: expand the indexed pixels, then convert the VGA palette to 6-bit DAC values. */
typedef struct {
    unsigned char prefix[12];
    short width;
    short height;
} PCXHeader;

typedef struct {
    unsigned char *pixels;
    unsigned char *palette;
    int width;
    int height;
    int pixel_format;
    int palette_entries;
    int allocation_size;
    int palette_size;
    int pixel_size;
} DecodedImage;

int decode_pcx_image(int source_address, int pixel_buffer_address, unsigned char *image_address)
{
    unsigned char run_value;
    PCXHeader *header;
    int source_address_cursor;
    unsigned int output_address_cursor;
    int encoded_byte;
    int width;
    int height;
    int pixel_count;
    int result;

    header = (PCXHeader *)source_address;
    source_address_cursor = source_address + 0x80;
    output_address_cursor = pixel_buffer_address;
    if (*(int *)header != 0x801050a) {
        result = 0xa01;
        return result;
    }
    width = header->width;
    height = header->height;
    pixel_count = width * height;
    /* PCX uses 0xC0|count,value runs and literal bytes below 0xC0. */
    while ((unsigned)(pixel_buffer_address + pixel_count) > output_address_cursor) {
        encoded_byte = *(unsigned char *)(source_address_cursor++);
        if ((encoded_byte & 0xc0) == 0xc0) {
            run_value = *(unsigned char *)(source_address_cursor++);
            for (encoded_byte &= 0x3f; encoded_byte > 0; encoded_byte--) {
                *(unsigned char *)(output_address_cursor++) = run_value;
            }
        } else {
            *(unsigned char *)(output_address_cursor++) = (signed char)encoded_byte;
        }
    }
    source_address_cursor++;
    output_address_cursor = pixel_buffer_address + pixel_count;
    for (encoded_byte = 0; encoded_byte < 0x300; encoded_byte++) {
        *(unsigned char *)(output_address_cursor++) =
        (unsigned char)(*(unsigned char *)(source_address_cursor++) >> 2);
    }
    g_pcx_allocation_size = pixel_count + 0x380;
    pcx_pixel_and_palette_payload_bytes = pixel_count + 0x300;
    ((DecodedImage *)image_address)->pixels = (unsigned char *)pixel_buffer_address;
    ((DecodedImage *)image_address)->width = width;
    ((DecodedImage *)image_address)->height = height;
    ((DecodedImage *)image_address)->palette_entries = 0x100;
    ((DecodedImage *)image_address)->pixel_format = 4;
    ((DecodedImage *)image_address)->palette_size = ((DecodedImage *)image_address)->palette_entries * 3;
    ((DecodedImage *)image_address)->pixel_size = ((DecodedImage *)image_address)->width * ((DecodedImage *)image_address)->height;
    ((DecodedImage *)image_address)->allocation_size = ((DecodedImage *)image_address)->palette_size + ((DecodedImage *)image_address)->pixel_size;
    ((DecodedImage *)image_address)->palette = ((DecodedImage *)image_address)->pixels + ((DecodedImage *)image_address)->pixel_size;
    result = 0;
    return result;
}
