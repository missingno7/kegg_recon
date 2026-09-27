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

unsigned char *g_font_bitmap_data;
struct TextRenderState g_text_render_state;
unsigned char *g_font_offsets_39;

typedef struct {
    int bitmap_offset;
    int reserved;
} FontGlyphOffset;

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

    default_metrics = (GlyphMetrics *)(g_font_bitmap_data + *(int *)(g_font_offsets_39 + 0x208));
    g_text_render_state.cursor_x = x;
    g_text_render_state.cursor_y = y;
    if (default_metrics->edge_x >= 0) goto default_x_ready;
    g_text_render_state.cursor_x -= default_metrics->edge_x;
default_x_ready:
    if (default_metrics->edge_y >= 0) goto read_character;
    g_text_render_state.cursor_y -= default_metrics->edge_y;
read_character:
    character = *(unsigned char *)(unsigned char *)(text_address++);
    switch (character) {
        goto scan_text_run;
        goto scan_text_run;
        case 9:
            if (g_text_render_state.font_mode != 1) goto tab_fixed_width;
            g_text_render_state.cursor_x += (default_metrics->advance + g_text_render_state.character_advance) << 3;
            goto tab_advance_complete;
        tab_fixed_width:
            g_text_render_state.cursor_x += g_text_render_state.character_advance << 3;
        tab_advance_complete:
            goto character_complete;
        case 32:
            g_text_render_state.cursor_x += g_text_render_state.character_advance;
            if (g_text_render_state.font_mode != 1) goto space_advance_complete;
            g_text_render_state.cursor_x += default_metrics->advance;
        space_advance_complete:
            goto character_complete;
        case 10:
        newline_control:
            g_text_render_state.cursor_x = g_text_render_state.clip_left;
            g_text_render_state.cursor_y += g_text_render_state.line_advance;
            if (g_text_render_state.font_mode != 1) goto newline_advance_complete;
            g_text_render_state.cursor_y += default_metrics->line_advance;
        newline_advance_complete:
            goto character_complete;
        case 18:
            goto character_complete;
        case 0:
            goto character_complete;
    }
scan_text_run:
    lookahead_address = text_address;
    scan_character = character;
    candidate_x = g_text_render_state.cursor_x;
measure_word_character:
    if (scan_character == 0x20) goto word_space;
    if (scan_character != 9) goto after_tab_check;
word_space:
    goto word_tab_check;
after_tab_check:
    if (scan_character != 0xa) goto after_newline_check;
word_tab_check:
    goto word_end_check;
after_newline_check:
    if (scan_character != 0x12) goto after_end_marker_check;
word_end_check:
    goto word_null_check;
after_end_marker_check:
    if (scan_character != 0) goto add_glyph_advance;
word_null_check:
    goto glyph_run_end;
add_glyph_advance:
    candidate_x += g_text_render_state.character_advance;
    if (g_text_render_state.font_mode != 1) goto check_word_fit;
    glyph = (GlyphMetrics *)(g_font_bitmap_data + *(int *)(g_font_offsets_39 + (scan_character << 3)));
    candidate_x += glyph->advance;
check_word_fit:
    if ((candidate_x - 1) <= g_text_render_state.clip_right) goto read_next_word_character;
    text_address += -1;
    goto newline_control;
read_next_word_character:
    scan_character = *(unsigned char *)(unsigned char *)(lookahead_address++);
    goto measure_word_character;
glyph_run_end:
    glyph = (GlyphMetrics *)(g_font_bitmap_data + *(int *)(g_font_offsets_39 + (character << 3)));
    if (((glyph->edge_x + (g_text_render_state.cursor_x + glyph->advance)) - 1) >
        g_text_render_state.clip_right)
        goto glyph_outside_clip;
    draw_bob_sprite_entry((int)glyph, g_text_render_state.cursor_x, g_text_render_state.cursor_y);
    g_text_render_state.cursor_x += g_text_render_state.character_advance;
    if (g_text_render_state.font_mode != 1) goto glyph_advance_complete;
    g_text_render_state.cursor_x += glyph->advance;
glyph_advance_complete:
    goto character_complete;
glyph_outside_clip:
    g_text_render_state.cursor_x = g_text_render_state.clip_left;
    g_text_render_state.cursor_y += g_text_render_state.line_advance;
    if (g_text_render_state.font_mode != 1) goto glyph_wrap_complete;
    g_text_render_state.cursor_y += default_metrics->line_advance;
glyph_wrap_complete:
    text_address--;
character_complete:
    if (g_text_render_state.cursor_y > g_text_render_state.clip_bottom) goto force_text_return;
    if (character != 0x12) goto check_string_terminator;
force_text_return:
    goto return_text_position;
check_string_terminator:
    if (character != 0) goto read_character;
return_text_position:
    if (character != 0) goto return_text_cursor;
    return 0;
return_text_cursor:
    return text_address;
}
void draw_zero_padded_number(int x, int y, int value, int base, int width)
{
    int zero_cursor;
    int zero_count;
    unsigned char buffer[32];

    ltoa(value, (int)((buffer + width) + 1), base);
    zero_count = width - strlen((int)((buffer + width) + 1));
    if (zero_count < 0)
        zero_count = 0;
    strcpy((int)(buffer + zero_count), (int)((buffer + width) + 1));
    zero_cursor = (int)buffer;
zero_padding_check:
    if (zero_count > 0) goto write_padding_zero;
    goto zero_padding_done;
zero_padding_decrement:
    zero_count--;
    goto zero_padding_check;
write_padding_zero:
    *(unsigned char *)zero_cursor++ = '0';
    goto zero_padding_decrement;
zero_padding_done:
    draw_text(x, y, (int)buffer);
}

void configure_text_renderer(unsigned char *glyph_offsets, unsigned char *bitmap_data,
                             char font_mode, int character_advance, int line_advance)
{
    g_font_offsets_39 = glyph_offsets;
    g_font_bitmap_data = bitmap_data;
    g_text_render_state.font_mode = font_mode;
    g_text_render_state.character_advance = character_advance;
    g_text_render_state.line_advance = line_advance;
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
    g_text_render_state.clip_left = left;
    g_text_render_state.clip_right = right;
    g_text_render_state.clip_top = top;
    g_text_render_state.clip_bottom = bottom;
}

int rectangles_intersect(const IntRect *first, const IntRect *second)
{
    if (first->left > second->right) goto no_intersection;
    if (first->top > second->bottom) goto no_intersection;
    if (first->right < second->left) goto no_intersection;
    if (first->bottom < second->top) goto no_intersection;
    return -1;
no_intersection:
    return 0;
}

void place_rect_from_offset(IntRect *rect, RectOffset size, RectOffset offset)
{
    rect->left += offset.delta_x;
    rect->top += offset.delta_y;
    rect->right = rect->left + size.delta_x;
    rect->bottom = rect->top + size.delta_y;
}

short rect_fits_viewport(RectOffset rect, RectOffset offset, int ignored,
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

    if ((second_left = second_x + second.offset_x) + second.width <
        (first_left = first_x + first.offset_x))
        goto no_overlap;
    if ((second_top = second_y + second.offset_y) + second.height <
        (first_top = first_y + first.offset_y))
        goto no_overlap;
    if (first.width + first_left < second_left)
        goto no_overlap;
    if (first.height + first_top < second_top)
        goto no_overlap;
    return -1;
no_overlap:
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

    if ((first_left = first_x + first.offset_x) + width >
        (second_left = second_x + second.offset_x) + second.width)
        goto no_overlap;
    if ((first_top = first_y + first.offset_y) + height >
        (second_top = second_y + second.offset_y) + second.height)
        goto no_overlap;
    if (first.width + first_left - inset_x < second_left)
        goto no_overlap;
    if (first.height + first_top - inset_y < second_top)
        goto no_overlap;
    return -1;
no_overlap:
    return 0;
}
