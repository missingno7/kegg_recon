/* These aliases keep descriptive names while preserving verified BSS linker names. */
#define previous_scan_code previous_key_scan_code
#define bios_keyboard_lock_flags keyboard_bios_status_flags
#define prior_key_ascii prior_key_ascii
#define KEY_HOOK_REPEAT_CHORD 0x01
#define KEY_HOOK_PAUSE 0x02
#define KEY_HOOK_SPACE_PRESS 0x04
#define KEY_HOOK_EXIT_CHORD 0x08
#define KEY_HOOK_ABORT_CHORD 0x10
#define KEY_HOOK_LOCK_LEDS 0x20
#define KEY_HOOK_CHORD_STATE 0x40
#define KEY_HOOK_CHEAT_CODE 0x80

#define PIC_MASTER_COMMAND_PORT 0x20
#define PIC_END_OF_INTERRUPT 0x20
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_LED_COMMAND 0xed
#define KEYBOARD_INPUT_BUFFER_FULL 0x02
#define BIOS_KEYBOARD_FLAGS_ADDRESS KE_LOWMEM(0x417) /* PORT: was 0x417 */
#define BIOS_KEYBOARD_BUFFER_HEAD_ADDRESS KE_LOWMEM(0x41a) /* PORT: was 0x41a */
#define BIOS_KEYBOARD_BUFFER_TAIL_ADDRESS KE_LOWMEM(0x41c) /* PORT: was 0x41c */
#define BIOS_KEYBOARD_SHIFT_FLAGS_MASK 0x70
#define BIOS_KEYBOARD_PRESERVED_FLAGS_MASK 0x8f
#define BIOS_KEYBOARD_LOCK_MASK 0x07
#define KEYBOARD_VALID_SCAN_MAX 0x7f
#define KEYBOARD_SCAN_RELEASE_MASK 0x80
#define KEYBOARD_INTERRUPT_REMAP_BASE 1
#define KEYBOARD_IRQ_LINE 9
#define KEYBOARD_EXIT_ACTION 0x101
#define KEYBOARD_CHEAT_TOGGLE_MASK 0xff
#define KEYBOARD_CONTROLLER_POLL_LIMIT 0x1388

/* TU T16: ignore_keyboard_action..request_keyboard_exit [0xf690, 0xffbe), code and owned data. */
#include <stdlib.h>
#include <i86.h>
#include <conio.h>
/* Keep raw scan-code and translated-key state in parallel bitmaps. */
enum KeyboardScanCode {
    SCAN_ESCAPE = 0x01,
    SCAN_BACKSPACE = 0x0e,
    SCAN_TAB = 0x0f,
    SCAN_GAME_PAUSE_KEY = 0x19,
    SCAN_ENTER = 0x1c,
    SCAN_LEFT_CONTROL = 0x1d,
    SCAN_GRAVE_ACCENT = 0x29,
    SCAN_LEFT_SHIFT = 0x2a,
    SCAN_RIGHT_SHIFT = 0x36,
    SCAN_LEFT_ALT = 0x38,
    SCAN_SPACE = 0x39,
    SCAN_CAPS_LOCK = 0x3a,
    SCAN_NUM_LOCK = 0x45,
    SCAN_SCROLL_LOCK = 0x46,
    SCAN_KEYPAD_DELETE = 0x53
};

enum KeyboardBitmapGroup {
    SCAN_BITMAP_GROUP_0,
    SCAN_BITMAP_GROUP_1,
    SCAN_BITMAP_GROUP_2,
    SCAN_BITMAP_GROUP_3,
    SCAN_BITMAP_GROUP_4,
    SCAN_BITMAP_GROUP_5
};

enum KeyboardBitmapMask {
    KEY_BITMAP_ESCAPE_LOW = 1 << (SCAN_ESCAPE & 7),
    KEY_BITMAP_TAB_WORD = 1 << (SCAN_TAB & 0x0f),
    KEY_BITMAP_CONTROL_HIGH = 1 << ((SCAN_LEFT_CONTROL & 0x0f) - 8),
    KEY_BITMAP_ALT_HIGH = 1 << ((SCAN_LEFT_ALT & 0x0f) - 8),
    KEY_BITMAP_KEYPAD_DELETE_LOW = 1 << (SCAN_KEYPAD_DELETE & 7),
    KEY_BITMAP_LEFT_SHIFT_HIGH = 1 << ((SCAN_LEFT_SHIFT & 0x0f) - 8),
    KEY_BITMAP_GRAVE_ACCENT_HIGH = 1 << ((SCAN_GRAVE_ACCENT & 0x0f) - 8),
    KEY_BITMAP_RIGHT_SHIFT_LOW = 1 << (SCAN_RIGHT_SHIFT & 7),
    KEY_BITMAP_BACKSPACE_HIGH = 1 << ((SCAN_BACKSPACE & 0x0f) - 8)
};

enum KeyboardLockFlag {
    KEYBOARD_SCROLL_LOCK_FLAG = 0x01,
    KEYBOARD_NUM_LOCK_FLAG = 0x02,
    KEYBOARD_CAPS_LOCK_FLAG = 0x04
};

enum KeyboardCheatSignatureIndex {
    CHEAT_TOGGLE_ALL_SIGNATURE_INDEX = 8,
    KEYBOARD_CHEAT_SIGNATURE_COUNT = 9
};

union KeyboardKeyBitmap {
    unsigned short word;
    struct {
        unsigned char low, high;
    }
    bytes;
};
union KeyboardHookFlags {
    unsigned short word;
    unsigned char bytes[2];
};
extern union KeyboardKeyBitmap scan_code_bitmap[8], translated_key_bitmap[8];
extern unsigned char current_ascii, pending_ascii_key, previous_scan_code, latest_scan_byte, current_scan_code, bios_keyboard_lock_flags, prior_key_ascii, keyboard_scan_byte;
void ignore_keyboard_action(void);
extern unsigned char keyboard_irq_line;
extern unsigned char keyboard_interrupt_number;
extern unsigned char picvec;
struct KeyboardInterruptRecord {
    short installation_status;
    unsigned char reserved[0x33];
};
/* The private BIOS keyboard buffer stores its head and tail words at +4 and +6. */
struct KeyboardBufferState {
    unsigned char reserved[4];
    unsigned short head_index;
    unsigned short tail_index;
};
extern struct KeyboardInterruptRecord key_irq;
extern void remove_keyboard_input_handler(void);
extern void save_irq(unsigned char *, int);
void install_keyboard_input_handler(void);
extern void restore(unsigned char *);
extern int keyboard_hook_flags;
extern int keyboard_mapped_address;
extern int dpmi_err;
extern void (*keyboard_handler_address)(void);
extern void (*keyboard_physical_start)(void);
extern void (*keyboard_mapping_state)(void);
extern void clear_keyboard_state(void);
extern void save_bios_keyboard_flags(void);
extern void install(int *);
void __interrupt keyboard_interrupt_handler(void);
extern void __far irq_110(void);
extern void restore_bios_keyboard_flags(void);
extern void update_key_state_from_scan_code(unsigned char);
void read_keyboard_scan_code(void);
extern int kbhit(void);
extern int getch(void);
void poll_keyboard(void);
extern void check_keyboard_cheat_code(void);
extern void handle_keyboard_repeat_chord(void);
extern void handle_pause_key(void);
extern void record_space_key_press(void);
extern void handle_keyboard_abort_chord(void);
extern void toggle_keyboard_lock_leds(void);
extern void update_keyboard_chord_state(void);
extern void (*key_repeat)(void);
extern void (*keyboard_release_handler)(void);
extern void (*key_action_hook)(int, int);
int wait_for_keyboard_controller(void);
extern void ignore_keyboard_action(void);
union KeyboardHookFlags hook_flags_word = {
    KEY_HOOK_SPACE_PRESS | KEY_HOOK_EXIT_CHORD | KEY_HOOK_ABORT_CHORD | KEY_HOOK_LOCK_LEDS
};
unsigned char keyboard_cheat_flags = 0;
unsigned char keyboard_reserved_bytes[2] = {
    0, 0
};
short keyboard_state = 0;
short keyboard_chord_state = 0;
short space_pressed = 0;
void (*key_repeat)(void) = ignore_keyboard_action;
void (*keyboard_release_handler)(void) = ignore_keyboard_action;
void (*key_action_hook)(int, int) = (void (*)(int, int))ignore_keyboard_action;
unsigned int keyboard_cheat_signatures[9] = {
    0x000b033d, 0x000a0315, 0x000902af, 0x00110512,
    0xffffffff, 0xffffffff, 0xffffffff, 0x0004012f, 0x0005017a
};
short bios_keyboard_flags_saved = 0;
char *bios_keyboard_buffer = "";
unsigned int keyboard_cheat_code_accumulator = 0;
unsigned char scan_code_to_ascii[128] = {
    0x3f, 0x3f, 0x26, 0x82, 0x22, 0x27, 0x28, 0xf5, 0x8a, 0x21, 0x87, 0x85, 0x29, 0x2d, 0x3f, 0x3f,
    0x41, 0x5a, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4f, 0x50, 0x3f, 0x3f, 0x3f, 0x3f, 0x51, 0x53,
    0x44, 0x46, 0x47, 0x48, 0x4a, 0x4b, 0x4c, 0x4d, 0x25, 0x3f, 0x3f, 0x5c, 0x57, 0x58, 0x43, 0x56,
    0x42, 0x4e, 0x3f, 0x3b, 0x3a, 0x2f, 0x3f, 0x2a, 0x3f, 0x20, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x37, 0x38, 0x39, 0x2d, 0x34, 0x35, 0x36, 0x2b, 0x31,
    0x32, 0x33, 0x30, 0x2e, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f
};
unsigned char keyboard_reserved_tail[3] = {
    0, 0, 0
};
void ignore_keyboard_action(void) {
}

void install_keyboard_input_handler(void) {
    keyboard_irq_line = KEYBOARD_IRQ_LINE;
    keyboard_interrupt_number = picvec + KEYBOARD_INTERRUPT_REMAP_BASE;
    save_irq((unsigned char *)&key_irq, (int)remove_keyboard_input_handler);
}

void remove_keyboard_input_handler(void) {
    restore((unsigned char *)&key_irq);
}

int initialize_keyboard_manager(int manager_flags) {
    int bios_buffer_address;
    if (manager_flags == 0) {
        manager_flags = keyboard_hook_flags;
        if (manager_flags == 0) return 0;
    }
    if (key_irq.installation_status != -1) {
        clear_keyboard_state();
        save_bios_keyboard_flags();
        keyboard_hook_flags = manager_flags;
        install_keyboard_input_handler();
        keyboard_handler_address = (void (*)(void))keyboard_interrupt_handler;
        keyboard_physical_start = (void (*)(void))irq_110;
        keyboard_mapping_state = (void (*)(void))((unsigned char __far *)irq_110 + 0x39);
        install((int *)&key_irq);
        if ((manager_flags & 1) == 1) {
            int bios_buffer_head_address;
            int bios_buffer_tail_address;
            bios_buffer_address = keyboard_mapped_address;
            bios_buffer_head_address = BIOS_KEYBOARD_BUFFER_HEAD_ADDRESS;
            ((struct KeyboardBufferState *)bios_buffer_address)->head_index = *(unsigned short *)bios_buffer_head_address;
            bios_buffer_tail_address = BIOS_KEYBOARD_BUFFER_TAIL_ADDRESS;
            ((struct KeyboardBufferState *)bios_buffer_address)->tail_index = *(unsigned short *)bios_buffer_tail_address;
            bios_keyboard_buffer = (char *)bios_buffer_address;
        }
        if (dpmi_err) return 0x501;
    }
    return 0;
}

void save_bios_keyboard_flags(void) {
    int bios_keyboard_flags_address;
    bios_keyboard_flags_address = BIOS_KEYBOARD_FLAGS_ADDRESS;
    if (bios_keyboard_flags_saved != -1) {
        bios_keyboard_lock_flags = *(unsigned char *)bios_keyboard_flags_address >> 4;
        atexit(restore_bios_keyboard_flags);
        bios_keyboard_flags_saved = -1;
    }
}

void restore_bios_keyboard_flags(void) {
    unsigned char *bios_keyboard_flags_address;
    bios_keyboard_flags_address = (unsigned char *)BIOS_KEYBOARD_FLAGS_ADDRESS;
    if (bios_keyboard_flags_saved == -1) {
        *bios_keyboard_flags_address = (*bios_keyboard_flags_address & BIOS_KEYBOARD_PRESERVED_FLAGS_MASK) | ((bios_keyboard_lock_flags << 4) & BIOS_KEYBOARD_SHIFT_FLAGS_MASK);
        bios_keyboard_flags_saved = 1;
    }
}

void clear_keyboard_state(void) {
    int bitmap_word_index;
    for (bitmap_word_index = 0; bitmap_word_index < 8; bitmap_word_index++) {
        scan_code_bitmap[bitmap_word_index].word = 0;
        translated_key_bitmap[bitmap_word_index].word = 0;
    }
}

void read_keyboard_scan_code(void) {
    update_key_state_from_scan_code((unsigned char)inp(KEYBOARD_DATA_PORT));
    if (*(unsigned char *)bios_keyboard_buffer) {
        update_key_state_from_scan_code(*(unsigned char *)bios_keyboard_buffer);
        *(unsigned char *)bios_keyboard_buffer = 0;
    }
}

void update_key_state_from_scan_code(unsigned char current_scan_code) {
    if ((current_scan_code & 0x6f) > 0x60) return;
    latest_scan_byte = current_scan_code;
    current_scan_code &= KEYBOARD_VALID_SCAN_MAX;
    pending_ascii_key = scan_code_to_ascii[current_scan_code];
    current_scan_code = (unsigned char)((int)current_scan_code >> 4);
    if (latest_scan_byte & KEYBOARD_SCAN_RELEASE_MASK) {
        scan_code_bitmap[current_scan_code].word = scan_code_bitmap[current_scan_code].word & (unsigned short)~(1 << (latest_scan_byte & 0x0f));
        current_scan_code = (unsigned char)((int)pending_ascii_key >> 4);
        translated_key_bitmap[current_scan_code].word = translated_key_bitmap[current_scan_code].word & (unsigned short)~(1 << (pending_ascii_key & 0x0f));
        pending_ascii_key |= latest_scan_byte & KEYBOARD_SCAN_RELEASE_MASK;
    }
    else {
        scan_code_bitmap[current_scan_code].word = scan_code_bitmap[current_scan_code].word | (unsigned short)(1 << (latest_scan_byte & 0x0f));
        current_scan_code = (unsigned char)((int)pending_ascii_key >> 4);
        translated_key_bitmap[current_scan_code].word = translated_key_bitmap[current_scan_code].word | (unsigned short)(1 << (pending_ascii_key & 0x0f));
    }
}

void poll_keyboard(void) {
    if (key_irq.installation_status != -1) {
        read_keyboard_scan_code();
        if (kbhit()) {
            pending_ascii_key = (unsigned char)getch();
            for (latest_scan_byte = 0; latest_scan_byte < KEYBOARD_VALID_SCAN_MAX; ++latest_scan_byte) {
                if (pending_ascii_key == scan_code_to_ascii[latest_scan_byte]) break;
            }
        }
    } else if (*(unsigned char *)bios_keyboard_buffer) {
        update_key_state_from_scan_code(*(unsigned char *)bios_keyboard_buffer);
        *(unsigned char *)bios_keyboard_buffer = 0;
    }
    if (previous_scan_code == keyboard_scan_byte || keyboard_scan_byte != latest_scan_byte) {
        prior_key_ascii = current_ascii;
        current_scan_code = keyboard_scan_byte;
    }
    keyboard_scan_byte = latest_scan_byte;
    current_ascii = pending_ascii_key;
    previous_scan_code = keyboard_scan_byte;
}

void dispatch_keyboard(void) {
    if (hook_flags_word.word & KEY_HOOK_CHEAT_CODE) check_keyboard_cheat_code();
    if ((hook_flags_word.word & KEY_HOOK_REPEAT_CHORD) == 1) handle_keyboard_repeat_chord();
    if (hook_flags_word.word & KEY_HOOK_PAUSE) handle_pause_key();
    if (hook_flags_word.word & KEY_HOOK_SPACE_PRESS) record_space_key_press();
    if (hook_flags_word.word & KEY_HOOK_ABORT_CHORD) handle_keyboard_abort_chord();
    if (hook_flags_word.word & KEY_HOOK_LOCK_LEDS) toggle_keyboard_lock_leds();
    if (hook_flags_word.word & KEY_HOOK_CHORD_STATE) update_keyboard_chord_state();
}

void record_space_key_press(void) {
    if (prior_key_ascii != 0x20 && current_ascii == 0x20) space_pressed = -1;
}

/* Keep the repeat callback active while Right Shift and Backspace remain down. */
void handle_keyboard_repeat_chord(void) {
    while ((scan_code_bitmap[SCAN_BITMAP_GROUP_3].bytes.low & KEY_BITMAP_RIGHT_SHIFT_LOW) && (scan_code_bitmap[SCAN_BITMAP_GROUP_0].bytes.high & KEY_BITMAP_BACKSPACE_HIGH)) {
        poll_keyboard();
        if (keyboard_state == 0) {
            key_repeat();
            keyboard_state = -1;
        }
    }
    if (keyboard_state != 0) {
        keyboard_release_handler();
        keyboard_state = 0;
    }
}

void handle_pause_key(void) {
    if (keyboard_scan_byte != SCAN_GAME_PAUSE_KEY) return;
    key_repeat();
    keyboard_state = -1;
    while (keyboard_scan_byte == SCAN_GAME_PAUSE_KEY) poll_keyboard();
    while (keyboard_scan_byte != SCAN_GAME_PAUSE_KEY) poll_keyboard();
    while (keyboard_scan_byte == SCAN_GAME_PAUSE_KEY) poll_keyboard();
    keyboard_release_handler();
    keyboard_state = 0;
}

void reset_keyboard_action_handlers(void) {
    key_repeat = ignore_keyboard_action;
    keyboard_release_handler = ignore_keyboard_action;
}

void update_keyboard_chord_state(void) {
    if ((scan_code_bitmap[SCAN_BITMAP_GROUP_2].bytes.high & KEY_BITMAP_LEFT_SHIFT_HIGH) && (scan_code_bitmap[SCAN_BITMAP_GROUP_2].bytes.high & KEY_BITMAP_GRAVE_ACCENT_HIGH)) keyboard_chord_state = 1;
    else keyboard_chord_state = 0;
}

/* Escape, Tab, and Left Control arm the exit request; Enter must be a fresh make-code. */
void handle_keyboard_exit_chord(void) {
    if (scan_code_bitmap[SCAN_BITMAP_GROUP_0].bytes.low & KEY_BITMAP_ESCAPE_LOW && (int)(short)scan_code_bitmap[SCAN_BITMAP_GROUP_0].word & KEY_BITMAP_TAB_WORD && scan_code_bitmap[SCAN_BITMAP_GROUP_1].bytes.high & KEY_BITMAP_CONTROL_HIGH && (current_scan_code != SCAN_ENTER && keyboard_scan_byte == SCAN_ENTER)) {
        key_action_hook(KEYBOARD_EXIT_ACTION,0);
    }
}

void handle_keyboard_abort_chord(void) {
    /* Ctrl+Alt+keypad Delete invokes the same exit action as the interrupt chord. */
    if ((scan_code_bitmap[SCAN_BITMAP_GROUP_1].bytes.high & KEY_BITMAP_CONTROL_HIGH) && (scan_code_bitmap[SCAN_BITMAP_GROUP_3].bytes.high & KEY_BITMAP_ALT_HIGH) && (scan_code_bitmap[SCAN_BITMAP_GROUP_5].bytes.low & KEY_BITMAP_KEYPAD_DELETE_LOW)) key_action_hook(KEYBOARD_EXIT_ACTION, 0);
}

void toggle_keyboard_lock_leds(void) {
    unsigned char previous_lock_flags=bios_keyboard_lock_flags;
    int scroll_lock_pressed,num_lock_pressed,caps_lock_pressed;
    if (current_scan_code!=SCAN_SCROLL_LOCK && keyboard_scan_byte==SCAN_SCROLL_LOCK) scroll_lock_pressed=1;
    else scroll_lock_pressed=0;
    if(scroll_lock_pressed) bios_keyboard_lock_flags^=KEYBOARD_SCROLL_LOCK_FLAG;
    if (current_scan_code!=SCAN_NUM_LOCK && keyboard_scan_byte==SCAN_NUM_LOCK) num_lock_pressed=1;
    else num_lock_pressed=0;
    if(num_lock_pressed) bios_keyboard_lock_flags^=KEYBOARD_NUM_LOCK_FLAG;
    if (current_scan_code!=SCAN_CAPS_LOCK && keyboard_scan_byte==SCAN_CAPS_LOCK) caps_lock_pressed=1;
    else caps_lock_pressed=0;
    if(caps_lock_pressed) bios_keyboard_lock_flags^=KEYBOARD_CAPS_LOCK_FLAG;
    if(previous_lock_flags!=bios_keyboard_lock_flags) {
        _disable();
        wait_for_keyboard_controller();
        outp(KEYBOARD_DATA_PORT,KEYBOARD_LED_COMMAND);
        wait_for_keyboard_controller();
        outp(KEYBOARD_DATA_PORT,bios_keyboard_lock_flags&BIOS_KEYBOARD_LOCK_MASK);
        _enable();
    }
}

/* Accumulate translated key codes and compare the completed sequence on Enter. */
void check_keyboard_cheat_code(void) {
    int signature_index;
    if (keyboard_scan_byte != current_scan_code && keyboard_scan_byte < 0x7f) {
        if (keyboard_scan_byte == SCAN_ENTER) {
            for (signature_index=0;(short)signature_index<KEYBOARD_CHEAT_SIGNATURE_COUNT;signature_index++) {
                if (keyboard_cheat_signatures[(short)signature_index] == keyboard_cheat_code_accumulator) {
                    if ((short)signature_index==CHEAT_TOGGLE_ALL_SIGNATURE_INDEX) keyboard_cheat_flags ^= KEYBOARD_CHEAT_TOGGLE_MASK;
                    else keyboard_cheat_flags ^= 1<<(short)signature_index;
                }
            }
            keyboard_cheat_code_accumulator=0;
        }
        else keyboard_cheat_code_accumulator += (unsigned short)current_ascii + 0x10000U;
    }
}

/* Poll the 8042 input-buffer-full bit with the original bounded retry count. */
int wait_for_keyboard_controller(void) {
    int poll_countdown=KEYBOARD_CONTROLLER_POLL_LIMIT;
    while (inp(KEYBOARD_STATUS_PORT)&KEYBOARD_INPUT_BUFFER_FULL && poll_countdown>0) {
        poll_countdown--;
    }
    if (poll_countdown>0) {
        return 0;
    }
    return -1;
}

void __interrupt keyboard_interrupt_handler(void) {
    copy_ds_to_es();
    read_keyboard_scan_code();
    outp(PIC_MASTER_COMMAND_PORT, PIC_END_OF_INTERRUPT);
    if (hook_flags_word.bytes[0] & KEY_HOOK_EXIT_CHORD)
    handle_keyboard_exit_chord();
}

void request_keyboard_exit(void) {
    key_action_hook(0x101, 0);
}

union KeyboardKeyBitmap translated_key_bitmap[8];
union KeyboardKeyBitmap scan_code_bitmap[8];
unsigned char current_ascii;
unsigned char pending_ascii_key;
unsigned char previous_scan_code;
unsigned char latest_scan_byte;
unsigned char current_scan_code;
unsigned char bios_keyboard_lock_flags;
unsigned char prior_key_ascii;
unsigned char keyboard_scan_byte;
