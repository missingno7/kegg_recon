#define TEXT_TAB 9
#define TEXT_LINE_FEED 10
#define TEXT_SPACE 32
#define TEXT_STOP_CHARACTER 0x12
#define FONT_MODE_PROPORTIONAL 1
#define DEFAULT_FONT_GLYPH_INDEX 0x41

/* Packed shared state; the gameplay view calls line_advance its v20d field. */
struct TextRenderState {
    unsigned char font_mode;
    int character_advance;
    int line_advance;
    int cursor_x;
    int cursor_y;
    int clip_left;
    int clip_right;
    int clip_top;
    int clip_bottom;
    unsigned char reserved[3];
};

unsigned char *font_bitmap_data;
struct TextRenderState text_render_state;

struct FontGlyphMetricOffset {
    int bitmap_offset;
    int reserved;
};

struct FontGlyphMetricOffset *font_glyph_metric_table;

typedef struct {
    short reserved0;
    short advance;
    short line_advance;
    unsigned char reserved6[4];
    short edge_x;
    short edge_y;
} GlyphMetrics;

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} IntRect;

typedef struct {
    short reserved0;
    short delta_x;
    short delta_y;
    short reserved1;
} RectOffset;

/* The BOB frame record supplies bounds and offsets; untouched shorts remain explicit. */
typedef struct {
    short reserved0;
    short width;
    short height;
    short reserved1;
    short reserved2;
    short offset_x;
    short offset_y;
    short reserved3;
    short reserved4;
} FrameMetrics;

extern void draw_bob_sprite_entry(int, int, int);
extern int strcpy(int, int);
extern int ltoa(int, int, int);
extern int strlen(int);

/* Control bytes move the text cursor; mode 1 uses each glyph's stored width and line metrics. */
int draw_text(int x, int y, int text_address)
{
    unsigned char character;
    int lookahead_address;
    int candidate_x;
    int scan_character;
    GlyphMetrics *glyph;
    GlyphMetrics *default_metrics;

    default_metrics = (GlyphMetrics *)(font_bitmap_data + font_glyph_metric_table[DEFAULT_FONT_GLYPH_INDEX].bitmap_offset);
    text_render_state.cursor_x = x;
    text_render_state.cursor_y = y;
    if (default_metrics->edge_x < 0) {
        text_render_state.cursor_x -= default_metrics->edge_x;
    }
    if (default_metrics->edge_y < 0) {
        text_render_state.cursor_y -= default_metrics->edge_y;
    }
    do {
        character = *(unsigned char *)(text_address++);
        switch (character) {
        case TEXT_TAB:
            if (text_render_state.font_mode == FONT_MODE_PROPORTIONAL) {
                text_render_state.cursor_x += (default_metrics->advance + text_render_state.character_advance) << 3;
            } else {
                text_render_state.cursor_x += text_render_state.character_advance << 3;
            }
            continue;
        case TEXT_SPACE:
            text_render_state.cursor_x += text_render_state.character_advance;
            if (text_render_state.font_mode == FONT_MODE_PROPORTIONAL) {
                text_render_state.cursor_x += default_metrics->advance;
            }
            continue;
        case TEXT_LINE_FEED:
newline_control:;
            text_render_state.cursor_x = text_render_state.clip_left;
            text_render_state.cursor_y += text_render_state.line_advance;
            if (text_render_state.font_mode == FONT_MODE_PROPORTIONAL) {
                text_render_state.cursor_y += default_metrics->line_advance;
            }
            continue;
        case TEXT_STOP_CHARACTER:
            continue;
        case 0:
            continue;
        }
        lookahead_address = text_address;
        scan_character = character;
        candidate_x = text_render_state.cursor_x;
        while (scan_character != 0x20 && scan_character != 9 && scan_character != 0xa && scan_character != 0x12 && scan_character) {
            candidate_x += text_render_state.character_advance;
            if (text_render_state.font_mode == FONT_MODE_PROPORTIONAL) {
                glyph = (GlyphMetrics *)(font_bitmap_data + font_glyph_metric_table[scan_character].bitmap_offset);
                candidate_x += glyph->advance;
            }
            if ((candidate_x - 1) > text_render_state.clip_right) {
                text_address += -1;
                /* Reuse the line-feed case's shared cursor update. */
                goto newline_control;
            }
            scan_character = *(unsigned char *)(lookahead_address++);
        }
        glyph = (GlyphMetrics *)(font_bitmap_data + font_glyph_metric_table[character].bitmap_offset);
        if (((glyph->edge_x + (text_render_state.cursor_x + glyph->advance)) - 1) <= text_render_state.clip_right) {
            draw_bob_sprite_entry((int)glyph, text_render_state.cursor_x, text_render_state.cursor_y);
            text_render_state.cursor_x += text_render_state.character_advance;
            if (text_render_state.font_mode == FONT_MODE_PROPORTIONAL) {
                text_render_state.cursor_x += glyph->advance;
            }
        } else {
            text_render_state.cursor_x = text_render_state.clip_left;
            text_render_state.cursor_y += text_render_state.line_advance;
            if (text_render_state.font_mode == FONT_MODE_PROPORTIONAL) {
                text_render_state.cursor_y += default_metrics->line_advance;
            }
            text_address--;
        }
    } while (text_render_state.cursor_y <= text_render_state.clip_bottom && character != TEXT_STOP_CHARACTER && character);
    if (!character) {
        return 0;
    }
    return text_address;
}
void draw_zero_padded_number(int x, int y, int value, int base, int width)
{
    int zero_cursor;
    int zero_count;
    unsigned char buffer[32];

    ltoa(value, (int)((buffer + width) + 1), base);
    zero_count = width - strlen((int)((buffer + width) + 1));
    if (zero_count < 0) {
        zero_count = 0;
    }
    strcpy((int)(buffer + zero_count), (int)((buffer + width) + 1));
    for (zero_cursor = (int)buffer; zero_count > 0; zero_count--) {
        *(unsigned char *)zero_cursor++ = '0';
    }
    draw_text(x, y, (int)buffer);
}

void configure_text_renderer(struct FontGlyphMetricOffset *glyph_metric_offsets, unsigned char *bitmap_data,
                             char font_mode, int character_advance, int line_advance)
{
    font_glyph_metric_table = glyph_metric_offsets;
    font_bitmap_data = bitmap_data;
    text_render_state.font_mode = font_mode;
    text_render_state.character_advance = character_advance;
    text_render_state.line_advance = line_advance;
}

void set_text_clip_rect(int left, int top, int right, int bottom)
{
    int swap;

    if (left > right) {
        swap = left;
        left = right;
        right = swap;
    }
    if (top > bottom) {
        swap = top;
        top = bottom;
        bottom = swap;
    }
    text_render_state.clip_left = left;
    text_render_state.clip_right = right;
    text_render_state.clip_top = top;
    text_render_state.clip_bottom = bottom;
}

int rectangles_intersect(const IntRect *first, const IntRect *second)
{
    if (first->left <= second->right) {
        if (first->top <= second->bottom) {
            if (first->right >= second->left) {
                if (first->bottom >= second->top) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

void place_rect_from_offset(IntRect *rect, RectOffset size, RectOffset offset)
{
    rect->left += offset.delta_x;
    rect->top += offset.delta_y;
    rect->right = rect->left + size.delta_x;
    rect->bottom = rect->top + size.delta_y;
}

/* The third argument is part of the original entry signature but is unused. */
short rect_fits_viewport(RectOffset rect, RectOffset offset, int unused_argument,
                         int x, int y, int right, int bottom, int max_x, int max_y)
{
    int rect_right;
    int rect_bottom;

    if ((rect_right = offset.delta_x + x) <= max_x)
        if ((rect_bottom = offset.delta_y + y) <= max_y)
            if (rect.delta_x + rect_right >= right)
                if (rect.delta_y + rect_bottom >= bottom)
                    return -1;
    return 0;
}

short frames_intersect(FrameMetrics first, int first_x, int first_y,
                       FrameMetrics second, int second_x, int second_y)
{
    int first_left;
    int second_top;
    int second_left;
    int first_top;

    if ((second_left = second_x + second.offset_x) + second.width >= (first_left = first_x + first.offset_x)) {
        if ((second_top = second_y + second.offset_y) + second.height >= (first_top = first_y + first.offset_y)) {
            if (first.width + first_left >= second_left) {
                if (first.height + first_top >= second_top) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

short frames_intersect_inset(FrameMetrics first, int first_x, int first_y,
                             FrameMetrics second, int second_x, int second_y,
                             int width, int height, int inset_x, int inset_y)
{
    int second_top;
    int first_left;
    int second_left;
    int first_top;

    if ((first_left = first_x + first.offset_x) + width <= (second_left = second_x + second.offset_x) + second.width) {
        if ((first_top = first_y + first.offset_y) + height <= (second_top = second_y + second.offset_y) + second.height) {
            if (first.width + first_left - inset_x >= second_left) {
                if (first.height + first_top - inset_y >= second_top) {
                    return -1;
                }
            }
        }
    }
    return 0;
}
