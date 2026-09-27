unsigned char sprite_frame_offset_scratch[8];
int high_score_record_index;
int high_score_char_index;
/* Strings and glyph tables used by the title, menu, score and instruction screens. */
#include <process.h>
unsigned char current_spell_effect_handler[16];
int high_score_checksum_byte;

#include <stdlib.h>
#include <string.h>

#define HIGH_SCORE_VISIBLE_ROWS 5
#define HIGH_SCORE_LAST_VISIBLE_ROW (HIGH_SCORE_VISIBLE_ROWS - 1)
#define HIGH_SCORE_NAME_LENGTH 8
#define HIGH_SCORE_NAME_LAST_INDEX 7
#define HIGH_SCORE_FILE_SIZE 150
#define DEFAULT_GAME_LIVES 4
#define RESTART_CODE_TEXT_OFFSET 0x2b
#define RESTART_CODE_TERMINATOR_OFFSET 0x2f
#define RESTART_CODE_XOR_KEY 0x7b69
#define RESTART_CODE_VALUE_MASK 0x3f
#define RESTART_CODE_DECADE_MASK 0x07
#define RESTART_CODE_PARITY_MASK 0x01
#define RESTART_CODE_NIBBLE_SHIFT 4
#define RESTART_CODE_ROTATE_BACK_SHIFT 12
#define RESTART_CODE_CHECK_SHIFT 2
#define RESTART_CODE_LIVES_SHIFT 10
#define RESTART_CODE_DECADE_SHIFT 6
#define RESTART_CODE_PARITY_SHIFT 9
#define SCAN_CODE_BACKSPACE 0x0e
#define SCAN_CODE_ENTER 0x1c
#define SCAN_CODE_ESCAPE 0x01
#define SCAN_CODE_SPACE 0x39

typedef struct GlyphRecord GlyphRecord;

#pragma pack(1)
#pragma pack()
typedef struct { unsigned char b[5]; } RestartCodeBuffer;
typedef struct { unsigned char bytes[9]; } HighScoreName;

#pragma pack(1)
typedef struct GameBall {
    int x;
    int y;
    int velocity_x;
    int velocity_y;
    unsigned char behavior;
    unsigned char flags;
} GameBall;
#pragma pack()

typedef struct Racket {
    int x;
    int y;
    int previous_x;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int sprite_frame_index;
    int sprite_height;
    int reward_level;
    int capture_timer;
    int state_2c;
    int state_30;
    int effect_state;
    int horizontal_recenter_timer;
    int vertical_recenter_timer;
    int effect_timer_40;
    int effect_frame_44;
    int animation_timer_48;
    int animation_step_4c;
    int animation_timer_50;
    int animation_step_54;
    int shield_frame_timer;
    int shield_frame_index;
    int spell_anim_timer;
    int spell_anim_index;
    int spell_parameter_68;
    int spell_parameter_6c;
    int spell_parameter_70;
    int previous_x_snapshot;
    int sprite_pointer;
} Racket;

typedef struct GameProgressState {
    int progress_marker;
    int life_balance;
    int opaque_08;
    int opaque_0c;
    int opaque_10;
    int score;
    int tail;
} GameProgressState;

typedef struct PlayerInputFlags {
    unsigned char control_flags;
    unsigned char spell_flags;
    unsigned char reserved_2;
    unsigned char reserved_3;
} PlayerInputFlags;

#pragma pack(1)
typedef struct TimedLevelChange {
    int cell_index;
    int ticks_remaining;
    unsigned char replacement_tile;
} TimedLevelChange;
#pragma pack()

typedef struct DisplayModeInfo {
    short render_state;
    unsigned char plane_addresses_or_transform_a[16];
    unsigned char page_offsets_or_transform_b[16];
    unsigned char page_adjustments_or_transform_c[16];
    unsigned char page_mode_classes[4];
    int buffer_size_or_draw_parameter;
    int row_stride;
    int resolution_height;
    int screen_width;
    int screen_height;
    int viewport_left;
    int viewport_top;
    int viewport_right_or_width;
    int viewport_bottom_or_height;
    unsigned char mode_flags;
    unsigned char sequencer_plane_mask;
    unsigned char graphics_read_map;
    unsigned char reserved_vga_byte;
    unsigned char saved_video_mode;
    unsigned char graphics_controller_mode;
    unsigned char render_cache_60;
    unsigned char render_cache_61;
    unsigned char render_cache_62;
    unsigned char tail;
} DisplayModeInfo;

typedef struct TextRenderState {
    unsigned char font_mode;
    int character_advance;
    int line_advance;
    int cursor_x;
    int cursor_y;
    int clip_left;
    int clip_right;
    int clip_top;
    int clip_bottom;
    unsigned char reserved_tail[3];
} TextRenderState;

#pragma pack(1)
typedef struct SpriteDrawCommand {
    int sprite_or_frame;
    short x;
    short y;
    short flags;
} SpriteDrawCommand;

typedef struct HighScoreRecord {
    HighScoreName name;
    unsigned score;
    unsigned factor_a;
    unsigned factor_b;
    unsigned checksum;
} HighScoreRecord;
#pragma pack()

extern unsigned char sndirq[];
extern unsigned char key_irq[];
extern unsigned char g_756f[];
extern void f_9afc(void);
extern void f_9b44(int);
extern void release_sound_system(void);
extern int configure_sound_dma(int);
extern void set_audio_transfer_mode(int);
extern void save_bios(void);
extern void restore_bios(void);
extern void clear_vga_palette(void);
extern void remove_keyboard_input_handler(void);
extern int initialize_keyboard_manager(int);
void launch_print_order_form(void);
extern int ordering_information_text;
extern int main_palette_fn;
extern int order_image_filename;
extern int order_sprite_filename;
extern int information_data_filename;
extern short image_color_depth;
extern short sound_blaster_detected;
extern int palette_fade_first_index;
extern int palette_fade_end_index;
extern short drawpage;
extern short page2;
extern short hook_flags_word;
extern short space_pressed;
extern unsigned short mouse_btn;
extern unsigned short mouse_btn_old;
extern int file_error_state;
extern int file_operation_result;
extern unsigned char screen_palette_buffer[];
extern unsigned char front_page_bufs[];
extern unsigned char back_page_queue[];
extern unsigned char sprite_commands[];
extern unsigned char *vga_buffer_base;
extern int sfx_data_ptr;
extern char *sprite_memory_base;
extern unsigned char *game_sprite_base;
extern int palette_cycle_offset;
extern int palette_cycle_delay;
extern int palette_entries;
extern int menu_result;
extern int pic_of;
extern DisplayModeInfo vga_state;
extern unsigned char current_scan_code;
extern unsigned char prior_key_ascii;
extern unsigned char keyboard_scan_byte;
extern unsigned char current_ascii;
extern int g_e4c8;
extern int g_e4d0_wfmxdlyju;
extern unsigned current_file_name;
extern void set_mouse_bounds(int, int, int, int);
extern int load_next_file(void *);
extern void fatal_exit(unsigned, unsigned);
extern void write_dac_palette(void *, int, int, int);
extern void handle_s_key(void);
extern void refresh_video_pages(int);
extern void adjust(void);
extern void set_page(void);
extern void apply_palette_gradients(void *, void *);
extern void f_9d40(unsigned char);
extern int load_picture_keep();
extern void plot_transformed_pixel(void *, int);
extern void draw_text(int, int, int);
extern void configure_text_renderer(int, int, unsigned char, int, int);
extern void set_text_clip_rect(int, int, int, int);
extern void queue_audio(int, int, int, int);
extern void set_image_pages(int, int, short, int, int);
extern int set_display_mode();
extern void fade_dac(void *, int, int, int);
void run_title_screen_loop(void);
extern int stop_audio_stream();
void load_title_screen_assets(void);
void draw_title_screen(void);
extern int menu_image_filename;
extern int menu_sprite_filename;
extern int menu_data_filename;
extern int version_text;
extern char *last_code_prompt;
extern int prompt_timeout;
extern int menu_frame;
extern GlyphRecord *menu_sprite_frame_table;
extern int restart_code_entry_state;
extern unsigned char codeok;
extern void draw_restart_code_text(int);
extern short image_buffer_error_code;
extern void update_menu_sprite_animation(void);
extern void queue_menu_sprite(void);
extern void handle_menu_input(void);
extern void cycle_menu_palette(void);
extern void handle_restart_code_input(void);
extern void redraw_image_region(int, int);
extern void show_page(void);
extern int next_packed_table_value(void *, void *);
extern SpriteDrawCommand *image_buffer_cursor;
extern short mouse_y_mean_recent;
extern short mouse_x_average_recent;
extern unsigned char keypad_x_min_bounds[];
extern unsigned char keypad_x_max_bounds[];
extern unsigned char keypad_y_min_bounds[];
extern unsigned char keypad_y_max_bounds[];
extern int temp;
extern unsigned char done;
union KeyboardKeyBitmap { unsigned short word; struct { unsigned char lo, hi; } bytes; };
extern union KeyboardKeyBitmap scan_code_bitmap[8];
extern short history_mouse_x_0;
extern short mouse_y_sample_0;
extern int invalid_code_text;
extern int valid_code_text;
extern unsigned char *enter_code_prompt;
extern unsigned char *text_entry_buffer;
extern unsigned int scratch;
extern unsigned char ptrbuf[];
extern unsigned char input_char;
extern int code_index;
extern int prompt;
extern unsigned char arcade;
int decode_restart_code(unsigned long encoded_restart_code);
extern short src_page;
extern short dst_page;
extern void copy_clipped_screen_rectangle(int, int, int, int, int, int, int, int);
extern unsigned int parity;
extern unsigned int decade;
extern unsigned int restart_word;
extern unsigned char lives;
extern unsigned char start_decade;
extern GameProgressState *score_state;
extern unsigned char level_number;
char *itoa(int, char *, int);
extern int screen_timer;
extern void copy_screen_span_entry(int, int, int, int, int);
void load_game_over_assets(void);
void prepare_game_over_background(void);
void draw_high_score_table(void);
void insert_high_score(void);
void enter_high_score_name(void);
void wait_for_key_or_click(void);
extern int draw_page();
extern int score_image_filename;
extern int font_sprite_filename;
extern int score_data_filename;
extern int brick_art_start;
extern int spell_art_base;
extern int enemy_picture;
extern int monster_art;
extern int player_shot_total;
extern HighScoreRecord high_score_records[];
extern unsigned char high_score_values[];
extern int hall_of_fame_text;
extern int result;
extern unsigned char high_score_cursor;
extern TextRenderState text_render_state;
extern unsigned minimum_high_score;
extern unsigned points;
extern char *strcpy(char *, const char *);
extern int enter_name_text;
extern int enjoy_yourself_text;
extern int cursor_direction;
extern int high_score_name_timer;
extern unsigned char high_score_input_redraw_flag;
extern void update_mouse(void);
extern int saved_high_score_state;
extern char *high_score_filename;
extern int f_1065b(char *, void *);
extern void validate_high_score_records(void);
void write_file_buffer(int a, int b, int c);
extern unsigned char high_score_factor_a[];
extern unsigned char high_score_factor_b[];
extern unsigned char high_score_checksums[];
extern int opening_credits_page;
extern int how_to_play_page;
extern int enemies_and_shields_page;
extern int mouse_and_high_score_page;
extern int programming_credits_page;
void load_instruction_assets(void);
void prepare_instruction_background(void);
void show_instruction_page(int instruction_text);
extern int brick_sprite_fn;
extern int spell_sprite_fn;
extern int foe_sprite_fn;
extern int racket_sprite_filename;
extern int digit_sprite_filename;
extern int monster_sprite_filename;
extern int monster_anim_fname;
extern int main_screen_data_filename;
extern int scoref;
extern int end_fn;
extern int game_title_text;
extern short page_idx;
extern int ending_page_one;
extern int ending_page_two;
extern int ending_page_three;
extern int ending_page_four;
extern int ending_page_five;
void load_ending_assets(void);
void prepare_ending_background(void);
void show_ending_page(int ending_text);
extern int score_background_filename;
extern int score_font_filename;
extern int end_screen_data_filename;
extern int publisher_data_filename;
extern int publisher_image_one_filename;
extern int publisher_image_two_filename;
extern int publisher_image_three_filename;
void set_vga_palette_rgb(unsigned char, unsigned char, unsigned char, unsigned char);
extern int fill_sprite_data;
void load_shared_game_assets(void);
void render_image_with_options(int, int, int, int, int);
extern unsigned char *transition_track_data;
extern int main_menu_return_transition_tracks;
void load_return_screen_assets(void);
void prepare_game_asset_read(void);
void await_input(int);
extern int file_mark;
extern int tile_art_base;
extern int fill_sprite_filename;
extern int game_font_filename;
extern int level_data_cursor;
extern int level_screen_data_filename;
extern int pause_screen_data_filename;
extern int game_over_data_filename;
extern short page3;
extern int tileid;
void clear_draw_page(int);
void draw_background_tiles(void);
extern unsigned char bonus_index;
extern int enemy_spawn_wait_time;
extern int remaining_brick_count;
extern unsigned short *cell_cursor;
extern unsigned int current_brick_code;
extern int portal_start_x;
extern int portal_y_source;
extern int portal_exit_xpos;
extern int portal_destination_y;
extern int sprite_metadata;
extern int bonus_stage_enemy_intervals[];
extern int bricks[];
extern unsigned char brick_code_map;
extern unsigned char spell_slots[];
extern unsigned char brick_code_mapping[];
extern unsigned char brick_sprite_offsets_first[];
void f_13889(int, int, int);
void draw_bob_sprite(int, int, int);
extern int tick;
void init_stage_palette(void);
void advance_tracks(void);
void queue_draws(void);
extern int sprite_base;
extern int level_palette_transition_tracks;
extern int path_count;
extern int palette_base;
void init_tracks(void);
void write_vga_palette(int);
extern int level_number_transition_tracks_a;
extern int tracks;
void write_level_number_glyphs(void);
extern Racket *racket_object;
void prep_level(void);
void move_mouse_to(int, int);
extern char monster_font_glyph_metrics[];
void draw_zero_padded_number(int, int, int, int, int);
extern int next_extra_life_score;
void submit_audio_request(int);
extern unsigned char frames;
extern char brick_sprite_offsets_remaining[];
void copy_tile_to_page(int, int);
void spawn_animated_sprite(int, int, int, int, int, int, int);
void spawn_falling_spell(unsigned char, unsigned char, int, int);
void draw_level_tile_on_pages(int a, int b, int c);
extern int timed_change_count;
void tick_level_change_queue(void);
extern TimedLevelChange *timed_change_cursor;
extern int timed_change_records;
extern int timed_event_cursor;
void apply_timed_level_change(void);
extern int work_value;
extern int g_68b7[];
extern int spell_count;
extern PlayerInputFlags *player_key_flags;
void update_racket_movement_bounds(void);
void move_falling_spells(void);

/* T02 initialized _DATA, in contribution order; values transcribed from obj3. */
struct GlyphRecord { int offset; int count; };

GlyphRecord menu_sprite_animation_frames[91] = {
    {4212, 1},
    {3704, 1},
    {3196, 1},
    {2772, 1},
    {2350, 1},
    {2004, 1},
    {1658, 1},
    {1338, 1},
    {1060, 1},
    {806, 1},
    {594, 1},
    {400, 1},
    {242, 1},
    {104, 1},
    {2, 1},
    {104, 1},
    {242, 1},
    {400, 1},
    {594, 1},
    {806, 1},
    {1060, 1},
    {1338, 1},
    {1658, 1},
    {2004, 1},
    {2350, 1},
    {2772, 1},
    {3196, 1},
    {3704, 1},
    {4212, 1},
    {4796, 1},
    {5380, 1},
    {6000, 1},
    {6668, 1},
    {7370, 1},
    {8124, 1},
    {8910, 1},
    {9744, 1},
    {10610, 1},
    {11528, 1},
    {12478, 1},
    {13476, 1},
    {14508, 1},
    {15584, 1},
    {16706, 1},
    {17864, 1},
    {16706, 1},
    {15584, 1},
    {14508, 1},
    {13476, 1},
    {12478, 1},
    {11528, 1},
    {10610, 1},
    {9744, 1},
    {8910, 1},
    {8124, 1},
    {7370, 1},
    {6668, 1},
    {6000, 1},
    {5380, 1},
    {4796, 1},
    {4212, 1},
    {19056, 1},
    {19698, 1},
    {20340, 1},
    {21040, 1},
    {21768, 1},
    {22526, 1},
    {23340, 1},
    {24182, 1},
    {25050, 1},
    {25952, 1},
    {26884, 1},
    {27868, 1},
    {28858, 1},
    {29904, 1},
    {30992, 1},
    {29904, 1},
    {28858, 1},
    {27868, 1},
    {26884, 1},
    {25952, 1},
    {25050, 1},
    {24182, 1},
    {23340, 1},
    {22526, 1},
    {21768, 1},
    {21040, 1},
    {20340, 1},
    {19698, 1},
    {19056, 1},
    {0, -90},
};

GlyphRecord menu_font_glyph_metrics[257] = {
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {35634, 1},
    {2, 1},
    {35688, 0},
    {35856, 0},
    {35990, 0},
    {36134, 0},
    {36280, 0},
    {36426, 0},
    {36574, 0},
    {36718, 0},
    {36860, 0},
    {37010, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {37156, 0},
    {37318, 0},
    {37464, 0},
    {37624, 0},
    {37778, 0},
    {37924, 0},
    {38068, 0},
    {38240, 0},
    {38398, 0},
    {38522, 0},
    {38662, 0},
    {38810, 0},
    {38952, 0},
    {39128, 0},
    {39300, 0},
    {39474, 0},
    {39618, 0},
    {39794, 0},
    {39940, 0},
    {40088, 0},
    {40224, 0},
    {40394, 0},
    {40556, 0},
    {40746, 0},
    {40914, 0},
    {41070, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {32094, 0},
    {32224, 0},
    {32378, 0},
    {32502, 0},
    {32660, 0},
    {32788, 0},
    {32924, 0},
    {33086, 0},
    {33240, 0},
    {33382, 0},
    {33562, 0},
    {33710, 0},
    {33842, 0},
    {33970, 0},
    {34096, 0},
    {34224, 0},
    {34378, 0},
    {34536, 0},
    {34646, 0},
    {34758, 0},
    {34874, 0},
    {35000, 0},
    {35116, 0},
    {35248, 0},
    {35374, 0},
    {35518, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {0, -256},
};

GlyphRecord large_title_glyph_metrics[257] = {
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {57274, 1},
    {2, 1},
    {57456, 0},
    {57964, 0},
    {58252, 0},
    {58748, 0},
    {59242, 0},
    {59686, 0},
    {60184, 0},
    {60680, 0},
    {61096, 0},
    {61592, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {44590, 0},
    {45084, 0},
    {45576, 0},
    {46034, 0},
    {46540, 0},
    {47034, 0},
    {47474, 0},
    {47978, 0},
    {48474, 0},
    {48960, 0},
    {49450, 0},
    {49936, 0},
    {50336, 0},
    {50844, 0},
    {51362, 0},
    {51870, 0},
    {52322, 0},
    {52832, 0},
    {53338, 0},
    {53836, 0},
    {54290, 0},
    {54798, 0},
    {55304, 0},
    {55810, 0},
    {56314, 0},
    {56778, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {44590, 0},
    {45084, 0},
    {45576, 0},
    {46034, 0},
    {46540, 0},
    {47034, 0},
    {47474, 0},
    {47978, 0},
    {48474, 0},
    {48960, 0},
    {49450, 0},
    {49936, 0},
    {50336, 0},
    {50844, 0},
    {51362, 0},
    {51870, 0},
    {52322, 0},
    {52832, 0},
    {53338, 0},
    {53836, 0},
    {54290, 0},
    {54798, 0},
    {55304, 0},
    {55810, 0},
    {56314, 0},
    {56778, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {0, -256},
};

GlyphRecord small_text_glyph_metrics[305] = {
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {7028, 1},
    {2, 1},
    {7176, 0},
    {7370, 0},
    {7514, 0},
    {7712, 0},
    {7900, 0},
    {8082, 0},
    {8280, 0},
    {8476, 0},
    {8656, 0},
    {8852, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 0},
    {272, 0},
    {542, 0},
    {814, 0},
    {1084, 0},
    {1356, 0},
    {1628, 0},
    {1902, 0},
    {2168, 0},
    {2442, 0},
    {2716, 0},
    {2980, 0},
    {3246, 0},
    {3512, 0},
    {3778, 0},
    {4050, 0},
    {4312, 0},
    {4584, 0},
    {4852, 0},
    {5126, 0},
    {5404, 0},
    {5674, 0},
    {5944, 0},
    {6210, 0},
    {6480, 0},
    {6754, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 0},
    {272, 0},
    {542, 0},
    {814, 0},
    {1084, 0},
    {1356, 0},
    {1628, 0},
    {1902, 0},
    {2168, 0},
    {2442, 0},
    {2716, 0},
    {2980, 0},
    {3246, 0},
    {3512, 0},
    {3778, 0},
    {4050, 0},
    {4312, 0},
    {4584, 0},
    {4852, 0},
    {5126, 0},
    {5404, 0},
    {5674, 0},
    {5944, 0},
    {6210, 0},
    {6480, 0},
    {6754, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {0, -256},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {57274, 1},
    {2, 1},
};

struct digit_entry { int value; int other; };
struct digit_entry level_digit_sprite_records[10] = {
    {10626, 0},
    {12054, 0},
    {12764, 0},
    {14190, 0},
    {15568, 0},
    {16774, 0},
    {18188, 0},
    {19620, 0},
    {20682, 0},
    {22124, 0},
};

GlyphRecord glyph_metrics_198[198] = {
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {44590, 0},
    {45084, 0},
    {45576, 0},
    {46034, 0},
    {46540, 0},
    {47034, 0},
    {47474, 0},
    {47978, 0},
    {48474, 0},
    {48960, 0},
    {49450, 0},
    {49936, 0},
    {50336, 0},
    {50844, 0},
    {51362, 0},
    {51870, 0},
    {52322, 0},
    {52832, 0},
    {53338, 0},
    {53836, 0},
    {54290, 0},
    {54798, 0},
    {55304, 0},
    {55810, 0},
    {56314, 0},
    {56778, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {44590, 0},
    {45084, 0},
    {45576, 0},
    {46034, 0},
    {46540, 0},
    {47034, 0},
    {47474, 0},
    {47978, 0},
    {48474, 0},
    {48960, 0},
    {49450, 0},
    {49936, 0},
    {50336, 0},
    {50844, 0},
    {51362, 0},
    {51870, 0},
    {52322, 0},
    {52832, 0},
    {53338, 0},
    {53836, 0},
    {54290, 0},
    {54798, 0},
    {55304, 0},
    {55810, 0},
    {56314, 0},
    {56778, 0},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {2, 1},
    {0, -256},
};

struct Grad { short x, y; unsigned char r0, g0, b0, r1, g1, b1; };
struct Grad order_info_palette_gradient[8] = {
    {0, 63, 0, 0, 63, 0, 0, 0},
    {0, 63, 0, 0, 0, 63, 0, 0},
    {0, 63, 63, 0, 0, 0, 0, 0},
    {0, 63, 0, 0, 0, 0, 0, 32},
    {0, 63, 0, 0, 32, 32, 0, 0},
    {0, 63, 32, 0, 0, 0, 0, 63},
    {0, 63, 0, 0, 63, 0, 0, 0},
    {-1, 0, 0, 0, 0, 0, 0, 0},
};

extern char *game_credit, *pc_credits, *pub_credit, *mouse_speed_help, *mouse_button_help, *return_to_dos_help;
extern char *restart_code_help, *enter_code_help, *validate_code_help, *author_message_text, *last_code_prompt;

char **restart_code_prompt_messages[11] = {
    &game_credit,
    &pc_credits,
    &pub_credit,
    &mouse_speed_help,
    &mouse_button_help,
    &return_to_dos_help,
    &restart_code_help,
    &enter_code_help,
    &validate_code_help,
    &author_message_text,
    &last_code_prompt,
};
char *level_table_filename = "ke_ldcwc.tab";
/* Only the zero-initialized state word at 0x5c7c is known; no C access was found. */
int g_5c7c = 0;

extern unsigned char keyboard_cheat_flags;
extern int (*key_repeat)();
extern int (*keyboard_release_handler)();
extern int current_ball_count;
extern unsigned char life_lost_flag;
extern int g_e1b8;
extern short g_e1bc;
extern short g_e1c0;
extern void init_game(void);
extern void next_lvl(void);
extern void update_racket_state(void);
extern void init_round(void);
extern void start_racket_release_animation(void);
extern void update_enemy_projectiles(void);
extern void load_palette(void);
extern void load_enemy(void);
extern void shot_cd(void);
extern void animate(void);
extern void handle_keyboard_controls(void);
extern void update(void);
extern void reset_keyboard_action_handlers(void);
void run_gameplay_session(void);
extern unsigned char difficulty_tier_index;
extern unsigned char score_storage[];
extern void set_game_progress(int, int, int);
extern int random_in_range(int, int);
extern int run_level(void);
extern int g_6230;
extern int sprite_instance_count;
extern unsigned char player_key_flag_storage[];
extern int enemy_cursor;
extern GameBall *current_ball_pointer;
extern int motion_dir;
extern int moving_target_count;
extern int enemy_timer;
extern unsigned char racket_state_storage[];
extern unsigned char *game_art_base;
extern void advance_racket_anim(void);
extern void start_spell_animation_e(void);
extern void start_racket_movement_animation(void);
extern void update_racket_dimensions(void);
extern void spawn_game_ball(int, int);
extern void initialize_main_menu(void);
void load_menu_graphics(void);
void prep_menu_background(void);
void wait_menu_select(void);
extern int title_image_file;
extern int title_data_filename;
extern short audio_stream_flag;
extern char *best_of_the_bests_text;
extern char *immortality_cheat_phrase;
void run_main_menu(void);

void run_main_menu(void) {
    unsigned int selected_menu_item;
    done = 0;
    tileid = -1;
    lives = DEFAULT_GAME_LIVES;
    start_decade = 0;
    codeok = 0;
    arcade = 0;
    prompt = -1;
    initialize_main_menu();
    do {
        update_menu_screen();
        selected_menu_item = (unsigned int)menu_result - 1;
        switch (selected_menu_item) {
        case 0:
            run_gameplay_session();
            if (level_number >= 60 && score_state->life_balance >= 0)
                show_ending_pages();
            points = score_state->score;
            show_game_over_screen();
            break;
        case 1:
            points = 0;
            show_game_over_screen();
            break;
        case 2:
            show_instructions();
            break;
        case 3:
            show_high_score_screen();
            break;
        default:
            break;
        }
    } while (done == 0);
    write_high_score_table();
}

void initialize_main_menu(void)
{
    stop_audio_stream();
    load_menu_graphics();
    set_display_mode(5);
    prep_menu_background();
    wait_menu_select();
    stop_audio_stream();
}

void load_menu_graphics(void)
{
    g_e4d0_wfmxdlyju = file_error_state;
    sprite_memory_base = (unsigned char *)g_e4d0_wfmxdlyju;
    file_operation_result = load_picture_keep(title_image_file);
    if (file_operation_result) {
        fatal_exit(file_operation_result, title_image_file);
    }
    if ((short)sound_blaster_detected == -1) {
        sfx_data_ptr = g_e4d0_wfmxdlyju;
        file_operation_result = load_next_file(title_data_filename);
        if (file_operation_result) {
            fatal_exit(file_operation_result, title_data_filename);
        }
        queue_audio(sfx_data_ptr, g_e4c8, 8000, 0);
    }
}

void prep_menu_background(void) {
    vga_buffer_base = sprite_memory_base + vga_state.buffer_size_or_draw_parameter;
    fade_dac(vga_buffer_base, 0, -63, -1);
    page_idx = src_page;
    plot_transformed_pixel(sprite_memory_base, page_idx);
    draw_page(page_idx);
    fade_dac(vga_buffer_base, -63, 0, 1);
}

void wait_menu_select(void)
{
    hook_flags_word = (hook_flags_word & 0xfffe) & 0xfffd;
    space_pressed = 0;
    do {
        do {
            f_9d40(3);
            handle_s_key();
            /* Input leaves both nested polling loops at once. */
            if (space_pressed || (mouse_btn != mouse_btn_old && mouse_btn) || (current_scan_code != keyboard_scan_byte && keyboard_scan_byte == SCAN_CODE_ESCAPE)) goto menu_wait_complete;
        } while (!sound_blaster_detected);
    } while (audio_stream_flag == -1);
menu_wait_complete:;
    fade_dac(vga_buffer_base, 0, 0x3f, 3);
}

void launch_print_order_form(void)
{
    int saved_state_756f;
    int saved_keyboard_irq_state;
    int saved_sound_irq_state;
    saved_state_756f = *(short *)g_756f;
    saved_keyboard_irq_state = *(short *)key_irq;
    saved_sound_irq_state = *(short *)sndirq;
    set_audio_transfer_mode(0);
    f_9afc();
    remove_keyboard_input_handler();
    release_sound_system();
    restore_bios();
    spawnlp(0, (char *)("", "printord.bat"), (char *)"", 0);
    clear_vga_palette();
    save_bios();
    if (saved_state_756f == -1) {
        f_9b44(0);
    }
    if (saved_keyboard_irq_state == -1) {
        initialize_keyboard_manager(0);
    }
    if (saved_sound_irq_state == -1) {
        configure_sound_dma(0);
    }
    set_audio_transfer_mode(-1);
}

void show_high_score_screen(void)
{
    unsigned char saved_video_page;
    int saved_palette_state;
    for (;;) {
        set_display_mode(2);
        g_e4d0_wfmxdlyju = (unsigned char *)file_error_state;
        sprite_memory_base = g_e4d0_wfmxdlyju;
        file_operation_result = load_picture_keep(order_image_filename);
        if (file_operation_result) {
            fatal_exit(file_operation_result, current_file_name);
        }
        game_sprite_base = g_e4d0_wfmxdlyju;
        file_operation_result = load_next_file((void *)order_sprite_filename);
        if (file_operation_result) {
            fatal_exit(file_operation_result, current_file_name);
        }
        vga_buffer_base = g_e4d0_wfmxdlyju + 0x900;
        file_operation_result = load_next_file((void *)main_palette_fn);
        if (file_operation_result) {
            fatal_exit(file_operation_result, current_file_name);
        }
        if (sound_blaster_detected == -1) {
            sfx_data_ptr = (int)g_e4d0_wfmxdlyju;
            file_operation_result = load_next_file((void *)information_data_filename);
            if (file_operation_result) {
                fatal_exit(file_operation_result, current_file_name);
            }
            queue_audio(sfx_data_ptr, g_e4c8, 0x1d4c, -1);
        }
        saved_video_page = *(signed char *)&drawpage;
        saved_palette_state = image_color_depth;
        clear_vga_palette();
        plot_transformed_pixel(sprite_memory_base, page2);
        configure_text_renderer((int)menu_font_glyph_metrics, (int)game_sprite_base, 1, 1, 3);
        set_text_clip_rect((int)(vga_state.viewport_left + 6), (int)(vga_state.viewport_top + 4), vga_state.viewport_right_or_width - 6, vga_state.viewport_bottom_or_height - 2);
        drawpage = page2;
        image_color_depth = 8;
        draw_text(8, 8, ordering_information_text);
        drawpage = (unsigned short)saved_video_page;
        image_color_depth = (unsigned short)saved_palette_state;
        refresh_video_pages(pic_of);
        palette_fade_first_index = 1;
        palette_fade_end_index = 0xbf;
        fade_dac((void *)(pic_of + 3), 0, -0x3f, -3);
        fade_dac(vga_buffer_base + 3, -0x3f, 0, 3);
        set_mouse_bounds((int)vga_state.viewport_left, (int)vga_state.viewport_top, vga_state.viewport_right_or_width, 0xc4);
        palette_cycle_delay = 0xa;
        palette_cycle_offset = 0;
        palette_entries = 0x1c0;
        apply_palette_gradients(order_info_palette_gradient, screen_palette_buffer);
        set_page();
        hook_flags_word = (((hook_flags_word & 0xfffe) & 0xfffd) & 0xfffb) & 0xff7f;
        menu_result = 0;
        space_pressed = 0;
        set_image_pages((int)sprite_commands, 0x100, 4, (int)front_page_bufs, (int)back_page_queue);
order_screen_poll:;
        f_9d40(3);
        if (prior_key_ascii == 0x50 || current_ascii != 0x50) break;
        launch_print_order_form();
    }
    adjust();
    handle_s_key();
    write_dac_palette((palette_cycle_offset * 3) + screen_palette_buffer, 0xc0, 0x40, 0);
    --palette_cycle_delay;
    if (!palette_cycle_delay) {
        palette_cycle_delay = 0xa;
        if ((palette_entries - 0x40) <= ++palette_cycle_offset) {
            palette_cycle_offset = 0;
        }
    }
    if ((prior_key_ascii == current_ascii || current_ascii != 0x20) && (current_scan_code == keyboard_scan_byte || keyboard_scan_byte != SCAN_CODE_ESCAPE)) {
        /* Re-poll this screen without reloading its assets. */
        if (mouse_btn == mouse_btn_old || !(*(unsigned char *)&mouse_btn & 1)) goto order_screen_poll;
    }
    palette_fade_first_index = 0;
    palette_fade_end_index = 0x100;
}

void update_menu_screen(void)
{
    stop_audio_stream();
    load_title_screen_assets();
    set_display_mode(2);
    draw_title_screen();
    run_title_screen_loop();
    stop_audio_stream();
}

void load_title_screen_assets(void)
{
    g_e4d0_wfmxdlyju = (unsigned char *)file_error_state;
    sprite_memory_base = g_e4d0_wfmxdlyju;
    file_operation_result = load_picture_keep(menu_image_filename);
    if (file_operation_result) {
        fatal_exit(file_operation_result, menu_image_filename);
    }
    game_sprite_base = g_e4d0_wfmxdlyju;
    file_operation_result = load_next_file((void *)menu_sprite_filename);
    if (file_operation_result) {
        fatal_exit(file_operation_result, menu_sprite_filename);
    }
    if (sound_blaster_detected == -1) {
        sfx_data_ptr = (int)g_e4d0_wfmxdlyju;
        file_operation_result = load_next_file((void *)menu_data_filename);
        if (file_operation_result) {
            fatal_exit(file_operation_result, menu_data_filename);
        }
        queue_audio(sfx_data_ptr, g_e4c8, 0x1f40, -1);
    }
    vga_buffer_base = g_e4d0_wfmxdlyju + 0xc00;
    file_operation_result = load_next_file((void *)main_palette_fn);
    if (file_operation_result) {
        fatal_exit(file_operation_result, main_palette_fn);
    }
    palette_entries = (g_e4c8 - 0xc00) / 3;
}

void draw_title_screen(void)
{
    unsigned char saved_video_page;
    int saved_palette_state;
    saved_video_page = *(signed char *)&drawpage;
    saved_palette_state = image_color_depth;
    clear_vga_palette();
    plot_transformed_pixel(sprite_memory_base, page2);
    configure_text_renderer((int)menu_font_glyph_metrics, (int)game_sprite_base, 1, 1, 3);
    set_text_clip_rect((int)vga_state.viewport_left, (int)vga_state.viewport_top, vga_state.viewport_right_or_width, vga_state.viewport_bottom_or_height);
    drawpage = page2;
    image_color_depth = 8;
    draw_text(0x10, 0x36, version_text);
    drawpage = (unsigned short)saved_video_page;
    image_color_depth = (unsigned short)saved_palette_state;
    refresh_video_pages((int)(sprite_memory_base + vga_state.buffer_size_or_draw_parameter));
    set_mouse_bounds((int)vga_state.viewport_left, (int)vga_state.viewport_top, vga_state.viewport_right_or_width, 0xc4);
    menu_frame = 1;
    menu_sprite_frame_table = menu_sprite_animation_frames;
    palette_cycle_delay = 2;
    palette_cycle_offset = 0;
    restart_code_entry_state = 0;
    prompt_timeout = 0;
    if (codeok) {
        draw_restart_code_text((int)last_code_prompt);
        prompt_timeout = 0x15e;
    }
    set_page();
}

void run_title_screen_loop(void)
{
    hook_flags_word = (((hook_flags_word & 0xfffe) & 0xfffd) & 0xfffb) & 0xff7f;
    menu_result = 0;
    space_pressed = 0;
    set_image_pages((int)sprite_commands, 0x100, 4, (int)front_page_bufs, (int)back_page_queue);
    while (menu_result == 0) {
        f_9d40(3);
        adjust();
        handle_s_key();
        update_menu_sprite_animation();
        queue_menu_sprite();
        handle_menu_input();
        cycle_menu_palette();
        handle_restart_code_input();
        redraw_image_region(0, 0);
        if (image_buffer_error_code != 0)
            fatal_exit(image_buffer_error_code, 0);
        show_page();
    }
}

void update_menu_sprite_animation(void) {
    next_packed_table_value(&menu_frame, &menu_sprite_frame_table);
}

void queue_menu_sprite(void) {
    image_buffer_cursor->sprite_or_frame = (int)(game_sprite_base + menu_sprite_frame_table->offset);
    image_buffer_cursor->x = mouse_x_average_recent;
    image_buffer_cursor->y = mouse_y_mean_recent;
    image_buffer_cursor->flags = 0;
    ++image_buffer_cursor;
}

void handle_menu_input(void)
{
    if ((current_scan_code != SCAN_CODE_ESCAPE && keyboard_scan_byte == SCAN_CODE_ESCAPE) || (mouse_btn_old != mouse_btn && mouse_btn && history_mouse_x_0 == (int)vga_state.viewport_left && mouse_y_sample_0 == (int)vga_state.viewport_top)) {
        done = 0xff;
        menu_result = -1;
    }
    for (temp = 0; temp < 4; ++temp) {
        if (history_mouse_x_0 > *(int *)(keypad_x_min_bounds + (temp << 4)) && history_mouse_x_0 < *(int *)(keypad_x_max_bounds + (temp << 4)) && mouse_y_sample_0 > *(int *)(keypad_y_min_bounds + (temp << 4)) && mouse_y_sample_0 < *(int *)(keypad_y_max_bounds + (temp << 4))) {
            update_menu_sprite_animation();
            if (!(scan_code_bitmap[1].bytes.hi & 0x20)) {
                if (mouse_btn && mouse_btn_old != mouse_btn) {
                    menu_result = temp + 1;
                }
            }
        }
    }
}

void cycle_menu_palette(void) {
    write_dac_palette(vga_buffer_base + palette_cycle_offset * 3, 0xc0, 0x20, 0);
    --palette_cycle_delay;
    if (palette_cycle_delay == 0) {
        palette_cycle_delay = 2;
        if (palette_entries - 0x20 <= ++palette_cycle_offset)
            palette_cycle_offset = 0;
    }
}

void handle_restart_code_input(void)
{
    if (current_scan_code != SCAN_CODE_SPACE && keyboard_scan_byte == SCAN_CODE_SPACE) {
        restart_code_entry_state = -1;
        code_index = 0;
        *(RestartCodeBuffer *)text_entry_buffer = *(RestartCodeBuffer *)"XXXX";
        strcpy((char *)(enter_code_prompt + RESTART_CODE_TEXT_OFFSET), (char *)text_entry_buffer);
        draw_restart_code_text((int)enter_code_prompt);
    }
    if (restart_code_entry_state) {
        if (keyboard_scan_byte != current_scan_code && !(keyboard_scan_byte & 0x80)) {
            input_char = keyboard_scan_byte;
            current_scan_code = input_char;
            if (input_char == SCAN_CODE_BACKSPACE) {
                if (code_index > 0) {
                    --code_index;
                    text_entry_buffer[code_index] = 0;
                }
                strcpy((char *)(enter_code_prompt + RESTART_CODE_TEXT_OFFSET), (char *)text_entry_buffer);
                enter_code_prompt[RESTART_CODE_TERMINATOR_OFFSET] = 0;
                draw_restart_code_text((int)enter_code_prompt);
            } else if (input_char == SCAN_CODE_ENTER) {
                code_index = -1;
                enter_code_prompt[RESTART_CODE_TERMINATOR_OFFSET] = 0;
            }
            input_char = current_ascii;
            prior_key_ascii = input_char;
            if ((input_char >= 0x41 && input_char <= 0x46) || (input_char >= 0x30 && input_char <= 0x39)) {
                text_entry_buffer[code_index] = input_char;
                text_entry_buffer[code_index + 1] = 0;
                if (code_index < 3) {
                    ++code_index;
                }
                strcpy((char *)(enter_code_prompt + RESTART_CODE_TEXT_OFFSET), (char *)text_entry_buffer);
                enter_code_prompt[RESTART_CODE_TERMINATOR_OFFSET] = 0;
                draw_restart_code_text((int)enter_code_prompt);
            }
        }
    } else {
        --prompt_timeout;
        if (prompt_timeout <= 0) {
            prompt_timeout = 0x15e;
            temp = 0xa;
            if (!codeok) {
                --temp;
            }
            ++prompt;
            if (prompt > temp) {
                arcade = 1;
                menu_result = 1;
                prompt = 0;
            }
            draw_restart_code_text(*(int *)*(unsigned char * *)((unsigned char *)restart_code_prompt_messages + (prompt << 2)));
        }
    }
    if (restart_code_entry_state && code_index == -1) {
        scratch = strtoul((char *)text_entry_buffer, (void *)ptrbuf, 0x10);
        if (decode_restart_code(scratch)) {
            draw_restart_code_text(valid_code_text);
        } else {
            draw_restart_code_text(invalid_code_text);
        }
        restart_code_entry_state = 0;
        prompt_timeout = 0x15e;
    }
}

void draw_restart_code_text(int text)
{
    unsigned char saved_video_page;
    int saved_palette_state;
    saved_video_page = *(signed char *)&drawpage;
    saved_palette_state = image_color_depth;
    configure_text_renderer((int)menu_font_glyph_metrics, (int)game_sprite_base, 1, 1, 3);
    set_text_clip_rect(0x10, 0xda, vga_state.viewport_right_or_width, vga_state.viewport_bottom_or_height);
    image_color_depth = 8;
    drawpage = src_page;
    copy_clipped_screen_rectangle(page2, 0, 0xd7, 0x13f, 0xef, drawpage, 0, 0xd7);
    draw_text(0x10, 0xda, text);
    drawpage = dst_page;
    copy_clipped_screen_rectangle(page2, 0, 0xd7, 0x13f, 0xef, drawpage, 0, 0xd7);
    draw_text(0x10, 0xda, text);
    image_color_depth = (unsigned short)saved_palette_state;
    drawpage = (unsigned short)saved_video_page;
}

int decode_restart_code(unsigned long encoded_restart_code)
{
    encoded_restart_code = (encoded_restart_code >> RESTART_CODE_NIBBLE_SHIFT) | (encoded_restart_code << RESTART_CODE_ROTATE_BACK_SHIFT);
    encoded_restart_code ^= RESTART_CODE_XOR_KEY;
    scratch = encoded_restart_code & RESTART_CODE_VALUE_MASK;
    decade = (encoded_restart_code >> RESTART_CODE_DECADE_SHIFT) & RESTART_CODE_DECADE_MASK;
    parity = (encoded_restart_code >> RESTART_CODE_PARITY_SHIFT) & RESTART_CODE_PARITY_MASK;
    restart_word = ~(((encoded_restart_code >> RESTART_CODE_LIVES_SHIFT) & RESTART_CODE_VALUE_MASK) ^ (~(decade + 2) << RESTART_CODE_CHECK_SHIFT)) & RESTART_CODE_VALUE_MASK;
    if (scratch == restart_word) {
        restart_word = ((decade ^ (((unsigned)scratch >> 1) ^ ((unsigned)scratch >> 3))) ^ (decade >> 1)) & 1;
        if (parity == restart_word) {
            lives = *(signed char *)&scratch;
            start_decade = *(unsigned char *)&decade;
        }
    } else {
        lives = DEFAULT_GAME_LIVES;
        start_decade = 0;
    }
    return start_decade;
}

void encode_restart_code(void)
{
    scratch = score_state->life_balance & RESTART_CODE_VALUE_MASK;
    decade = (level_number / 0xa) & 7;
    parity = ((decade ^ (((unsigned)scratch >> 1) ^ ((unsigned)scratch >> 3))) ^ (decade >> 1)) & 1;
    restart_word = ~((~(decade + 2) << RESTART_CODE_CHECK_SHIFT) ^ scratch) & RESTART_CODE_VALUE_MASK;
    restart_word = ((((restart_word << RESTART_CODE_LIVES_SHIFT) | (parity << RESTART_CODE_PARITY_SHIFT)) | (decade << RESTART_CODE_DECADE_SHIFT)) | scratch) ^ RESTART_CODE_XOR_KEY;
    restart_word = (restart_word << RESTART_CODE_NIBBLE_SHIFT) | (restart_word >> RESTART_CODE_ROTATE_BACK_SHIFT);
    lives = *(signed char *)&scratch;
    start_decade = *(unsigned char *)&decade;
    itoa(restart_word & 0xffff, (char *)(last_code_prompt + 0x32), 0x10);
}

void show_game_over_screen(void)
{
    stop_audio_stream();
    load_game_over_assets();
    set_display_mode(1);
    prepare_game_over_background();
    insert_high_score();
    drawpage = page2;
    draw_high_score_table();
    copy_screen_span_entry((short)page2, 0, (short)src_page, 0, vga_state.buffer_size_or_draw_parameter);
    copy_screen_span_entry((short)page2, 0, (short)dst_page, 0, vga_state.buffer_size_or_draw_parameter);
    drawpage = dst_page;
    draw_page((short)drawpage);
    enter_high_score_name();
    drawpage = page2;
    draw_high_score_table();
    copy_screen_span_entry((short)page2, 0, (short)src_page, 0, vga_state.buffer_size_or_draw_parameter);
    copy_screen_span_entry((short)page2, 0, (short)dst_page, 0, vga_state.buffer_size_or_draw_parameter);
    drawpage = src_page;
    show_page();
    if (image_buffer_error_code) {
        fatal_exit((short)image_buffer_error_code, 0);
    }
    screen_timer = 1400;
    wait_for_key_or_click();
    stop_audio_stream();
}

void load_game_over_assets(void)
{
    g_e4d0_wfmxdlyju = file_error_state;
    sprite_memory_base = g_e4d0_wfmxdlyju;
    load_picture_keep(score_image_filename);
    game_sprite_base = g_e4d0_wfmxdlyju;
    load_next_file(font_sprite_filename);
    if ((short)sound_blaster_detected == -1) {
        sfx_data_ptr = g_e4d0_wfmxdlyju;
        load_next_file(score_data_filename);
        queue_audio(sfx_data_ptr, g_e4c8, 8000, -1);
    }
}

void prepare_game_over_background(void)
{
    plot_transformed_pixel(sprite_memory_base, page2);
    refresh_video_pages((int)(sprite_memory_base + vga_state.buffer_size_or_draw_parameter));
    image_color_depth = 8;
}

void draw_high_score_table(void)
{
    int text_x;
    int text_y;
    int text_buffer;
    configure_text_renderer((int)large_title_glyph_metrics, (int)game_sprite_base, 0, 0x11, 0x17);
    set_text_clip_rect(vga_state.viewport_left, vga_state.viewport_top, vga_state.viewport_right_or_width, vga_state.viewport_bottom_or_height);
    draw_text(0x32, 0x10, hall_of_fame_text);
    text_y = 0x32;
    for (high_score_cursor = 1; high_score_cursor <= HIGH_SCORE_VISIBLE_ROWS; ++high_score_cursor) {
        text_buffer = (int)text_entry_buffer;
        itoa(high_score_cursor, (char *)text_buffer, 0xa);
        strcat((char *)text_buffer, (char *)".");
        text_x = 0xe;
        draw_text(text_x, text_y, text_buffer);
        text_y += text_render_state.line_advance;
    }
    text_y = 0x32;
    for (high_score_cursor = 0; high_score_cursor < HIGH_SCORE_VISIBLE_ROWS; ++high_score_cursor) {
        text_x = 0x36;
        text_buffer = (int)high_score_records[high_score_cursor].name.bytes;
        draw_text(text_x, text_y, text_buffer);
        text_y += text_render_state.line_advance;
    }
    text_y = 0x32;
    for (high_score_cursor = 0; high_score_cursor < HIGH_SCORE_VISIBLE_ROWS; ++high_score_cursor) {
        result = high_score_records[high_score_cursor].score;
        text_buffer = (int)text_entry_buffer;
        ltoa(result, (char *)text_buffer, 0xa);
        text_x = ((6 - strlen((char *)text_buffer)) * 0x11) + 0xd5;
        draw_text(text_x, text_y, text_buffer);
        text_y += text_render_state.line_advance;
    }
}

void insert_high_score(void)
{
    if (points >= minimum_high_score) {
        input_char = HIGH_SCORE_VISIBLE_ROWS;
        do {
            --input_char;
        } while (points >= high_score_records[input_char - 1].score && input_char > 0);
        high_score_cursor = HIGH_SCORE_LAST_VISIBLE_ROW;
        do {
            --high_score_cursor;
            if (input_char <= high_score_cursor) {
                strcpy((char *)&high_score_records[high_score_cursor + 1],
                       (char *)&high_score_records[high_score_cursor]);
                high_score_records[high_score_cursor + 1].score = high_score_records[high_score_cursor].score;
            }
        } while (high_score_cursor > 0);
        high_score_records[input_char].name = *(HighScoreName *)"        ";
        high_score_records[input_char].score = points;
        points = input_char;
        return;
    }
    points = 0xffffffff;
}

void enter_high_score_name(void)
{
    if (points != 0xffffffff) {
        high_score_cursor = 0;
        text_entry_buffer[0] = 0;
        high_score_input_redraw_flag = 0x80;
        high_score_name_timer = 0x24;
        cursor_direction = -1;
        do {
            --high_score_name_timer;
            if (!high_score_name_timer) {
                high_score_name_timer = 0x23;
                cursor_direction = -cursor_direction;
            }
            if (keyboard_scan_byte != current_scan_code) {
                input_char = keyboard_scan_byte;
                current_scan_code = input_char;
                if (input_char == SCAN_CODE_BACKSPACE) {
                    if (high_score_cursor) {
                        --high_score_cursor;
                        text_entry_buffer[high_score_cursor] = 0;
                    }
                    high_score_input_redraw_flag = 0x80;
                }
            }
            if (current_ascii != prior_key_ascii) {
                input_char = current_ascii;
                prior_key_ascii = input_char;
                if ((input_char >= 0x41 && input_char <= 0x5a) || (input_char >= 0x30 && input_char <= 0x39) || input_char == 0x20) {
                    text_entry_buffer[high_score_cursor] = input_char;
                    text_entry_buffer[high_score_cursor + 1] = 0;
                    if (high_score_cursor < HIGH_SCORE_NAME_LAST_INDEX) {
                        ++high_score_cursor;
                    }
                    high_score_input_redraw_flag = 0x80;
                }
            }
            if (high_score_input_redraw_flag) {
                high_score_input_redraw_flag = 0;
                copy_clipped_screen_rectangle(page2, 0xe, 0x32, vga_state.viewport_right_or_width, 0xa5, drawpage, 0xe, 0x32);
                draw_text(0x36, (text_render_state.line_advance * points) + 0x32, (int)text_entry_buffer);
            }
            if (high_score_name_timer == 0x23) {
                copy_clipped_screen_rectangle(page2, 0x24, 0xae, vga_state.viewport_right_or_width, 0xc5, drawpage, 0x24, 0xae);
                if (cursor_direction < 0) {
                    draw_text(0x24, 0xae, enter_name_text);
                }
            }
            f_9d40(1);
        } while (keyboard_scan_byte != SCAN_CODE_ENTER && keyboard_scan_byte != SCAN_CODE_ESCAPE);
        strcpy((char *)high_score_records[points].name.bytes, (char *)text_entry_buffer);
        points = 0xffffffff;
    }
    drawpage = page2;
    draw_text(0x32, 0xae, enjoy_yourself_text);
}

void wait_for_key_or_click(void)
{
    space_pressed = 0;
    *(unsigned char *)&hook_flags_word &= 0xfc;
    *(unsigned char *)&hook_flags_word |= 4;
    show_page();
    update_mouse();
    do {
        f_9d40(3);
        handle_s_key();
        if (space_pressed || (mouse_btn != mouse_btn_old && *(unsigned char *)&mouse_btn & 7) || (current_scan_code != keyboard_scan_byte && keyboard_scan_byte == SCAN_CODE_ESCAPE)) break;
        --screen_timer;
    } while (screen_timer);
}

void save_high_score_table(void)
{
    HighScoreRecord saved[HIGH_SCORE_VISIBLE_ROWS];
    for (temp = 0; temp < HIGH_SCORE_VISIBLE_ROWS; ++temp)
        saved[temp] = high_score_records[temp];
    validate_high_score_records();
    file_operation_result = f_1065b(high_score_filename, g_e4d0_wfmxdlyju);
    if (file_operation_result == 0) {
        validate_high_score_records();
        if (saved_high_score_state == result)
            return;
    }
    for (temp = 0; temp < HIGH_SCORE_VISIBLE_ROWS; ++temp)
        high_score_records[temp] = saved[temp];
}

void write_high_score_table(void)
{
    validate_high_score_records();
    validate_high_score_records();
    saved_high_score_state = result;
    write_file_buffer((int)high_score_filename, (int)g_e4d0_wfmxdlyju, HIGH_SCORE_FILE_SIZE);
}

void validate_high_score_records(void)
{
    g_e4d0_wfmxdlyju = (int)high_score_records;
    result = 0;
    for (high_score_record_index = 0; high_score_record_index < HIGH_SCORE_VISIBLE_ROWS; ++high_score_record_index) {
        result ^= ~(high_score_records[high_score_record_index].score << high_score_record_index);
        for (high_score_char_index = 0; high_score_char_index < HIGH_SCORE_NAME_LENGTH; ++high_score_char_index) {
            high_score_checksum_byte = high_score_records[high_score_record_index].name.bytes[high_score_char_index];
            result ^= high_score_checksum_byte << ((high_score_char_index % 8) << 2);
        }
    }
    for (temp = 0; temp < HIGH_SCORE_VISIBLE_ROWS; ++temp) {
        if (((high_score_records[temp].score * (high_score_records[temp].factor_b * high_score_records[temp].factor_a)) * 0x1b2c3e4f ^ -0x758ea2dc) != high_score_records[temp].checksum) {
            ++result;
        }
        high_score_records[temp].checksum = ((high_score_records[temp].score * (high_score_records[temp].factor_b * high_score_records[temp].factor_a)) * 0x1b2c3e4f) ^ -0x758ea2dc;
    }
}

void show_instructions(void)
{
    stop_audio_stream();
    load_instruction_assets();
    set_display_mode(1);
    prepare_instruction_background();
    show_instruction_page(opening_credits_page);
    show_instruction_page(how_to_play_page);
    show_instruction_page(enemies_and_shields_page);
    show_instruction_page(mouse_and_high_score_page);
    show_instruction_page(programming_credits_page);
    stop_audio_stream();
}

void load_instruction_assets(void)
{
    g_e4d0_wfmxdlyju = file_error_state;
    sprite_memory_base = g_e4d0_wfmxdlyju;
    load_picture_keep(scoref);
    load_next_file(main_palette_fn);
    game_sprite_base = g_e4d0_wfmxdlyju;
    load_next_file(end_fn);
    if ((short)sound_blaster_detected == -1) {
        sfx_data_ptr = g_e4d0_wfmxdlyju;
        load_next_file(information_data_filename);
        queue_audio(sfx_data_ptr, g_e4c8, 7500, -1);
    }
}

void prepare_instruction_background(void)
{
    plot_transformed_pixel(sprite_memory_base, (short)page2);
    refresh_video_pages((int)((sprite_memory_base + vga_state.buffer_size_or_draw_parameter) + 1536));
    image_color_depth = 8;
}

void show_instruction_page(int instruction_text)
{
    copy_screen_span_entry(page2, 0, src_page, 0, vga_state.buffer_size_or_draw_parameter);
    copy_screen_span_entry(page2, 0, dst_page, 0, vga_state.buffer_size_or_draw_parameter);
    drawpage = page_idx;
    configure_text_renderer((int)large_title_glyph_metrics, (int)game_sprite_base, 0, 0x11, 0x17);
    set_text_clip_rect(vga_state.viewport_left, vga_state.viewport_top, vga_state.viewport_right_or_width, vga_state.viewport_bottom_or_height);
    draw_text(0x40, 0xa, game_title_text);
    configure_text_renderer((int)small_text_glyph_metrics, (int)game_sprite_base, 0, 0xa, 0xd);
    set_text_clip_rect(vga_state.viewport_left, vga_state.viewport_top, vga_state.viewport_right_or_width, vga_state.viewport_bottom_or_height);
    draw_text(7, 0x1e, instruction_text);
    if (image_buffer_error_code) {
        fatal_exit(image_buffer_error_code, 0);
    }
    screen_timer = 0x834;
    wait_for_key_or_click();
}

void show_ending_pages(void)
{
    stop_audio_stream();
    set_display_mode(-1);
    load_ending_assets();
    prepare_ending_background();
    show_ending_page(ending_page_one);
    show_ending_page(ending_page_two);
    show_ending_page(ending_page_three);
    show_ending_page(ending_page_four);
    show_ending_page(ending_page_five);
    stop_audio_stream();
}

void load_ending_assets(void)
{
    g_e4d0_wfmxdlyju = file_error_state;
    sprite_memory_base = g_e4d0_wfmxdlyju;
    load_picture_keep(score_background_filename);
    load_next_file(main_palette_fn);
    if ((short)sound_blaster_detected == -1) {
        sfx_data_ptr = g_e4d0_wfmxdlyju;
        load_next_file(end_screen_data_filename);
        queue_audio(sfx_data_ptr, g_e4c8, 12000, -1);
    }
    game_sprite_base = g_e4d0_wfmxdlyju;
    load_next_file(score_font_filename);
}

void prepare_ending_background(void)
{
    plot_transformed_pixel(sprite_memory_base, page2);
    refresh_video_pages((int)(sprite_memory_base + vga_state.buffer_size_or_draw_parameter + 0x900));
    image_color_depth = 8;
}

void show_ending_page(int ending_text)
{
    copy_screen_span_entry(page2, 0, src_page, 0, vga_state.buffer_size_or_draw_parameter);
    copy_screen_span_entry(page2, 0, dst_page, 0, vga_state.buffer_size_or_draw_parameter);
    drawpage = page_idx;
    configure_text_renderer((int)large_title_glyph_metrics, (int)game_sprite_base, 0, 17, 23);
    set_text_clip_rect(vga_state.viewport_left, vga_state.viewport_top, vga_state.viewport_right_or_width, vga_state.viewport_bottom_or_height);
    draw_text(0x40, 10, game_title_text);
    configure_text_renderer((int)small_text_glyph_metrics, (int)game_sprite_base, 0, 10, 13);
    set_text_clip_rect(vga_state.viewport_left, vga_state.viewport_top, vga_state.viewport_right_or_width, vga_state.viewport_bottom_or_height);
    draw_text(7, 30, ending_text);
    if (image_buffer_error_code != 0)
        fatal_exit(image_buffer_error_code, 0);
    screen_timer = 0x5208;
    wait_for_key_or_click();
}

void run_screen_transition(void)
{
    int fade_step;
    int unused_stack_slot_a;
    int unused_stack_slot_b;
    char *saved_screen_buffer_a;
    char *saved_screen_buffer_b;

    stop_audio_stream();
    set_display_mode(1);
    g_e4d0_wfmxdlyju = file_error_state;
    fade_dac(vga_buffer_base, 0, 0x3f, 8);
    sprite_memory_base = (char *)g_e4d0_wfmxdlyju;
    vga_buffer_base = sprite_memory_base + vga_state.buffer_size_or_draw_parameter;
    load_picture_keep(publisher_image_one_filename);
    saved_screen_buffer_a = g_e4d0_wfmxdlyju;
    load_picture_keep(publisher_image_two_filename);
    saved_screen_buffer_b = g_e4d0_wfmxdlyju;
    load_picture_keep(publisher_image_three_filename);
    sfx_data_ptr = g_e4d0_wfmxdlyju;
    load_next_file(publisher_data_filename);
    queue_audio(sfx_data_ptr, g_e4c8, 0x2ae4, -1);
    plot_transformed_pixel(sprite_memory_base, page2);
    copy_screen_span_entry(page2, 0, src_page, 0, vga_state.buffer_size_or_draw_parameter);
    copy_screen_span_entry(page2, 0, dst_page, 0, vga_state.buffer_size_or_draw_parameter);

    for (fade_step = 0; fade_step < 0x40; fade_step++)
        set_vga_palette_rgb(1, (unsigned char)fade_step, (unsigned char)fade_step, (unsigned char)fade_step);
    for (fade_step = 0; fade_step < 0x15e; fade_step++)
        f_9d40(1);
    for (fade_step = 0; fade_step < 0x80; fade_step++)
        write_dac_palette(vga_buffer_base, 0x80, 0x80, 0x3f - fade_step / 2);
    fade_dac(vga_buffer_base, 0, -0x3f, -1);

    sprite_memory_base = saved_screen_buffer_a;
    vga_buffer_base = sprite_memory_base + vga_state.buffer_size_or_draw_parameter;
    plot_transformed_pixel(sprite_memory_base, page2);
    copy_screen_span_entry(page2, 0, src_page, 0, vga_state.buffer_size_or_draw_parameter);
    copy_screen_span_entry(page2, 0, dst_page, 0, vga_state.buffer_size_or_draw_parameter);
    fade_dac(vga_buffer_base, -0x3f, 0, 1);
    for (fade_step = 0; fade_step < 0x8c; fade_step++)
        f_9d40(1);
    fade_dac(vga_buffer_base, 0, -0x3f, -3);

    sprite_memory_base = saved_screen_buffer_b;
    vga_buffer_base = sprite_memory_base + vga_state.buffer_size_or_draw_parameter;
    plot_transformed_pixel(sprite_memory_base, page2);
    copy_screen_span_entry(page2, 0, src_page, 0, vga_state.buffer_size_or_draw_parameter);
    copy_screen_span_entry(page2, 0, dst_page, 0, vga_state.buffer_size_or_draw_parameter);
    fade_dac(vga_buffer_base, -0x3f, 0, 1);
    for (fade_step = 0; fade_step < 0x8c; fade_step++)
        f_9d40(1);
    fade_dac(vga_buffer_base, 0, -0x3f, -3);
}

void show_order_info(void)
{
    stop_audio_stream();
    load_shared_game_assets();
    queue_audio(sfx_data_ptr, g_e4c8, 0x1f40, -1);
    render_image_with_options(page_idx, 8, fill_sprite_data + 0x7102, 0xa0, 100);
}

void return_to_main_menu(void)
{
    copy_screen_span_entry(page_idx, 0, page2, 0, vga_state.buffer_size_or_draw_parameter);
    copy_screen_span_entry(page2, 0, src_page, 0, vga_state.buffer_size_or_draw_parameter);
    copy_screen_span_entry(page2, 0, dst_page, 0, vga_state.buffer_size_or_draw_parameter);
    stop_audio_stream();
    g_e4d0_wfmxdlyju = file_error_state;
    load_return_screen_assets();
    queue_audio(sfx_data_ptr, g_e4c8, 0x1f40, -1);
    transition_track_data = (unsigned char *)&main_menu_return_transition_tracks;
    prepare_game_asset_read();
    hook_flags_word = ((*(short *)&hook_flags_word) & 0xfffe) & 0xfffd;
    await_input(0x8ca);
    stop_audio_stream();
}

void load_assets(void)
{
    file_mark = g_e4d0_wfmxdlyju;
    tile_art_base = g_e4d0_wfmxdlyju;
    load_next_file(fill_sprite_filename);
    fill_sprite_data = g_e4d0_wfmxdlyju;
    load_next_file(game_font_filename);
    level_data_cursor = g_e4d0_wfmxdlyju;
    load_next_file(level_table_filename);
    if (g_e4c8 != 0x8958)
        file_error_state = 0;
    sfx_data_ptr = g_e4d0_wfmxdlyju;
    if (sound_blaster_detected == -1)
        load_next_file(level_screen_data_filename);
    g_e4d0_wfmxdlyju = file_mark;
}

void load_shared_game_assets(void)
{
    file_mark = g_e4d0_wfmxdlyju;
    fill_sprite_data = g_e4d0_wfmxdlyju;
    load_next_file(game_font_filename);
    sfx_data_ptr = g_e4d0_wfmxdlyju;
    if (sound_blaster_detected == -1)
        load_next_file(pause_screen_data_filename);
    g_e4d0_wfmxdlyju = file_mark;
}

void load_return_screen_assets(void)
{
    file_mark = g_e4d0_wfmxdlyju;
    fill_sprite_data = g_e4d0_wfmxdlyju;
    load_next_file(game_font_filename);
    sfx_data_ptr = g_e4d0_wfmxdlyju;
    if (sound_blaster_detected == -1)
        load_next_file(game_over_data_filename);
    g_e4d0_wfmxdlyju = file_mark;
}

void reset_level(void);

void load_and_draw_level(void);

int wait_level(void);

void init_stage_palette(void);

void entry_prompt(void);

void write_level_number_glyphs(void);

void redraw_level_state(void);

void prepare_game_asset_read(void);

void draw_status(void);

void update_game_status_panel(void);

void process_brick_hit(int a, int b, int c, int d);

void draw_level_tile_on_pages(int a, int b, int c);

void copy_tile_to_page(int a, int b);

void refresh_video_pages(int a);

void process_timed_level_changes(void);

void tick_level_change_queue(void);

void apply_timed_level_change(void);

void queue_timed_level_change(int a, int b);

void handle_gameplay_keypress(void);

void update_falling_spells(void);
