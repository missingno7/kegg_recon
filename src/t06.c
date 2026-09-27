/*
 * TIMER.C  -  timer, retrace and clock routines
 */
#include <i86.h>
#include <conio.h>
#define PIT_CONTROL_PORT 0x43
#define PIT_CHANNEL0_DATA_PORT 0x40
#define VGA_INPUT_STATUS_1_PORT 0x3da
#define VGA_VERTICAL_RETRACE_BIT 8
extern short windows_environment_detected;
extern int measure_pit_channel0(void);
extern unsigned char tmr_rec[];
extern unsigned char timer_num;
extern unsigned char pic_mask;
extern unsigned char picvec;
extern void reset_timer(void);
extern void save_irq(void *, int);
void install_timer_irq(void);
extern int set_pit_channel0_reload(int);
extern void restore(void *);
extern unsigned irq_flags;
extern int irq_handler;
extern int irq_phys;
extern int irq_map;
extern int irq_addr;
extern unsigned dpmi_err;
extern void __far pit_channel0_interrupt(void);
/* Run selected input/sprite hooks, then wait for a logical tick. */
void wait_for_tick(short wait_flags);
extern void clear_timer_events(void);
extern int install(void *);
extern void __far o2_103(void);
extern void __far o2_e0(void);
extern void (*kbd_irq_hook)(void);
extern void (*kbd_poll_hook)(void);
extern void (*mouse_update_hook)(void);
extern void (*sprite_update_hook)(void);
extern /* Poll VGA input status bit 3 across the vertical retrace. */
void wait_for_vsync(void);
extern /* Advance timer records and invoke due callbacks in registration order. */
void process_timer_events(void);

struct TimerEvent { void (*callback)(void); int period; int elapsed; };
int pit_tick_accumulator5;
struct TimerEvent timer_events4o[5];
/* Timer state whose role is not established by the callers. */
int timer_state_word;
int retrace_spin_count8;
int pit_counter_snapshot2v;
int timer_error_hundredths0z;
int timer_enabled09;
/* Saved image-buffer cursor used across a gameplay frame update. */
int save_cur;
/* Retraces consumed by the current gameplay interval. */
short retrace_tick_count;
short retrace_count5;
/* Retrace count captured as the gameplay interval's logical tick limit. */
short ticklim;

void noop_callback(void);
short keyboard_state_word = 0;
/* Set after PIT samples pass the timer calibration checks. */
short timer_ok = 0;
int pit_rollover_value = 0xffff;
unsigned timer_delta = 0xffff;
/* Initialized timer-area value has no recovered C/ASM references; purpose unknown. */
unsigned timer_data_word_73b0 = 0x445f0000UL;
short *kbd_state_ptr = &keyboard_state_word;
void (*kbd_irq_hook)(void) = noop_callback;
void (*kbd_poll_hook)(void) = noop_callback;
void (*mouse_update_hook)(void) = noop_callback;
void (*sprite_update_hook)(void) = noop_callback;
unsigned char *timer_sync_flag_ptr = "\0";
short timer_event_count = 0;
/* Initialized timer-area value has no recovered C/ASM references; purpose unknown. */
short timer_data_word_73ce = 0x6d20;
char *clock_update_message = "Please Wait, I am updating your clock...";






void noop_callback(void)
{
}




int verify_timer(void)
{
    int sample_index;
    int timer_sample;
    int previous_sample;
    int total_samples;
    previous_sample = 0;
    total_samples = 0;
    if (windows_environment_detected == -1) {
        timer_ok = 0;
        return timer_ok;
    }
    for (sample_index = 0; sample_index < 4; sample_index++) {
        timer_sample = measure_pit_channel0();
        total_samples += timer_sample;
        if (timer_sample > 0x61a8 || timer_sample < 0x2710) {
            timer_ok = 0;
            return timer_ok;
        }
        if (previous_sample) {
            previous_sample -= timer_sample;
            if (previous_sample < 0) {
                previous_sample = -previous_sample;
            }
            if ((previous_sample / 0x200) > 0) {
                timer_ok = 0;
                return timer_ok;
            }
        }
        previous_sample = timer_sample;
    }
    total_samples = (total_samples / 4) - measure_pit_channel0();
    if (total_samples < 0) {
        total_samples = -total_samples;
    }
    if (((total_samples * 4) / 0x200) > 0) {
        timer_ok = 0;
        return timer_ok;
    }
    timer_ok = -1;
    return timer_ok;
}

/* Timer routine. */

void install_timer_irq(void)
{
    pic_mask = 8;
    timer_num = picvec;
    save_irq(tmr_rec, (int)reset_timer);
}

/* Timer routine. */

void reset_timer(void)
{
    _disable();
    restore(tmr_rec);
    set_pit_channel0_reload(0);
    _enable();
    timer_delta = 0xffff;
    pit_rollover_value = timer_delta;
}




int start_timer(int timer_options)
{
    if (!timer_options) {
        timer_options = irq_flags;
        if (!timer_options) {
            return 0;
        }
    }
    if (timer_ok == -1 && *(short *)tmr_rec != -1) {
        timer_delta = measure_pit_channel0();
        if (timer_delta > 0x61a8 || timer_delta < 0x2710) {
            timer_ok = 0;
            return 0;
        }
        irq_flags = timer_options;
        install_timer_irq();
        pit_rollover_value = timer_delta - 0x100;
        clear_timer_events();
        timer_enabled09 = 1;
        retrace_count5 = 1;
        _disable();
        irq_handler = (int)pit_channel0_interrupt;
        irq_phys = (int)o2_e0;
        irq_map = (int)o2_103;
        install(tmr_rec);
        set_pit_channel0_reload(0xffff);
        _enable();
        if ((timer_options & 1) == 1) {
            timer_sync_flag_ptr = (unsigned char *)irq_addr;
        }
        if (dpmi_err) {
            return 0x502;
        }
        wait_for_tick(0);
        wait_for_tick(0);
    }
    return 0;
}




void timer_noop(void)
{
}




/* Latch PIT channel 0 and convert the elapsed count to hundredths. */
short read_pit_counter(void)
{
    unsigned short counter_low, counter_high;
    outp(PIT_CONTROL_PORT, 0);
    counter_low = inp(PIT_CHANNEL0_DATA_PORT);
    counter_high = inp(PIT_CHANNEL0_DATA_PORT);
    pit_tick_accumulator5 = pit_counter_snapshot2v;
    pit_counter_snapshot2v = ((unsigned short)counter_high << 8) + (unsigned short)counter_low;
    if (pit_counter_snapshot2v < pit_tick_accumulator5) {
    } else {
        pit_tick_accumulator5 += pit_rollover_value;
    }
    timer_error_hundredths0z = (pit_tick_accumulator5 - pit_counter_snapshot2v) * 100 / 0x4280;
    return timer_error_hundredths0z;
}




/* Run selected input/sprite hooks, then wait for a logical tick. */
void wait_for_tick(short wait_flags)
{
    int starting_tick;
    starting_tick = retrace_count5;
    if ((unsigned short)(wait_flags & 1) == 1) {
        (*kbd_irq_hook)();
    }
    if ((unsigned short)(wait_flags & 1) == 1) {
        (*kbd_poll_hook)();
    }
    if (wait_flags & 2) {
        (*mouse_update_hook)();
    }
    if (wait_flags & 4) {
        (*sprite_update_hook)();
    }
    if (*(short *)tmr_rec == -1) {
        retrace_spin_count8 = 0;
        while (retrace_count5 == starting_tick) {
            ++retrace_spin_count8;
            if (*(unsigned char *)timer_sync_flag_ptr) {
                ++retrace_count5;
                *(unsigned char *)timer_sync_flag_ptr = 0;
            }
        }
    } else {
        retrace_spin_count8 = 0;
        wait_for_vsync();
        process_timer_events();
    }
    ticklim = retrace_count5;
    retrace_count5 = 0;
}




/* Poll VGA input status bit 3 across the vertical retrace. */
void wait_for_vsync(void)
{
    while ((unsigned char)inp(VGA_INPUT_STATUS_1_PORT) & VGA_VERTICAL_RETRACE_BIT) {
        ++retrace_spin_count8;
    }
    while (!((unsigned char)inp(VGA_INPUT_STATUS_1_PORT) & VGA_VERTICAL_RETRACE_BIT)) {
        ++retrace_spin_count8;
    }
}

/* Timer routine. */

/* Advance timer records and invoke due callbacks in registration order. */
void process_timer_events(void)
{
    unsigned short event_index;
    if (!*(short *)kbd_state_ptr) {
        ++retrace_count5;
    }
    for (event_index = 0; (unsigned short)event_index < (unsigned short)timer_event_count; event_index++) {
        timer_events4o[event_index].elapsed += timer_delta;
        if ((unsigned)timer_events4o[event_index].elapsed >= timer_events4o[event_index].period) {
            timer_events4o[event_index].elapsed -= timer_events4o[event_index].period;
            timer_events4o[event_index].callback();
        }
    }
}

/* Timer routine. */

void schedule_timer_event(int callback_address, int period_ticks)
{
    if ((unsigned short)timer_event_count < 5) {
        timer_events4o[(unsigned short)timer_event_count].callback = (void (*)(void))callback_address;
        timer_events4o[(unsigned short)timer_event_count].period = period_ticks;
        timer_events4o[(unsigned short)timer_event_count].elapsed = 0;
        ++timer_event_count;
    }
}

/* Timer routine. */

void clear_timer_events(void)
{
    timer_event_count = 0;
}
/* Watcom -ot source-offset anchor (file[10336] = CP437 block):                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          Û*/
