#define decode_pcx_image f_a0e0 /* Keep T08's legacy import without changing its bytes. */
int g_pcx_allocation_size;
int g_e1c8;
int g_e1cc;
int g_pcx_pixels_d;

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
            encoded_byte &= 0x3f;
        run_length_check:
            if (encoded_byte > 0)
                goto write_run_byte;
            goto run_length_done;
        run_length_decrement:
            encoded_byte--;
            goto run_length_check;
        write_run_byte:
            *(unsigned char *)(output_address_cursor++) = run_value;
            goto run_length_decrement;
        run_length_done:
            ;
        } else {
            *(unsigned char *)(output_address_cursor++) = (signed char)encoded_byte;
        }
    }

    source_address_cursor++;
    output_address_cursor = pixel_buffer_address + pixel_count;
    encoded_byte = 0;
palette_entry_check:
    if (encoded_byte < 0x300)
        goto write_palette_entry;
    goto palette_entries_done;
palette_entry_increment:
    encoded_byte++;
    goto palette_entry_check;
write_palette_entry:
    *(unsigned char *)(output_address_cursor++) =
        (unsigned char)(*(unsigned char *)(source_address_cursor++) >> 2);
    goto palette_entry_increment;
palette_entries_done:
    ;

    g_pcx_allocation_size = pixel_count + 0x380;
    g_pcx_pixels_d = pixel_count + 0x300;
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
