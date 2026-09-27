/*
 * TIMER.C  -  timer, retrace and clock routines
 */
#include <i86.h>
#include <conio.h>
#pragma aux verify_timer "f_9974";
#pragma aux timer_calibrated "g_73a6";
#pragma aux start_timer "f_9b44";
#pragma aux reset_timer_counter "f_9afc";
#pragma aux wait_for_tick "f_9d40";
extern short windows_environment_detected;
extern int f_9f64(void);
extern unsigned char g_756f[];
extern unsigned char g_7585;
extern unsigned char g_7586;
extern unsigned char g_75a8;
extern void reset_timer_counter(void);
extern void f_d656(void *, int);
void install_timer_irq(void);
extern int set_pit_channel0_reload(int);
extern void f_d7b8(void *);
extern unsigned g_7588;
extern int g_758c;
extern int g_7590;
extern int g_7594;
extern int g_75a4;
extern unsigned g_75c4;
extern void __far pit_channel0_interrupt(void);
void wait_for_tick(short wait_flags);
extern void clear_timer_events(void);
extern int f_da01(void *);
extern void __far o2_103(void);
extern void __far o2_e0(void);
extern void (*kbd_irq_hook)(void);
extern void (*kbd_poll_hook)(void);
extern void (*mouse_update_hook)(void);
extern void (*sprite_update_hook)(void);
extern void wait_for_vsync(void);
extern void process_timer_events(void);

struct TimerEvent { void (*callback)(void); int period; int elapsed; };
int pit_tick_accumulator5;
struct TimerEvent timer_events4o[5];
/* Timer state; role not established. */
int g_e1a4_g;
int retrace_spin_count8;
int pit_counter_snapshot2v;
int timer_error_hundredths0z;
int timer_enabled09;
/* Shared timer state; role not established. */
int g_e1b8;
/* Shared timer state; role not established. */
short g_e1bc;
short retrace_count5;
/* Shared timer state; role not established. */
short g_e1c0;

void noop_callback(void);
short keyboard_state_word = 0;
short timer_calibrated = 0;
int pit_rollover_value = 0xffff;
unsigned timer_delta = 0xffff;
unsigned g_73b0 = 0x445f0000UL;
short *kbd_state_ptr = &keyboard_state_word;
void (*kbd_irq_hook)(void) = noop_callback;
void (*kbd_poll_hook)(void) = noop_callback;
void (*mouse_update_hook)(void) = noop_callback;
void (*sprite_update_hook)(void) = noop_callback;
unsigned char *timer_sync_flag_ptr = "\0";
short timer_event_count = 0;
short g_73ce = 0x6d20;
char *clock_update_message = "Please Wait, I am updating your clock...";



/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл noop_callback  empty default hook                                     лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void noop_callback(void)
{
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл verify_timer  measure the timer                                      лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

int verify_timer(void)
{
    int sample_index;
    int timer_sample;
    int previous_sample;
    int total_samples;
    previous_sample = 0;
    total_samples = 0;
    if (windows_environment_detected == -1) {
        timer_calibrated = 0;
        return timer_calibrated;
    }
    for (sample_index = 0; sample_index < 4; sample_index++) {
        timer_sample = f_9f64();
        total_samples += timer_sample;
        if (timer_sample > 0x61a8 || timer_sample < 0x2710) {
            timer_calibrated = 0;
            return timer_calibrated;
        }
        if (previous_sample) {
            previous_sample -= timer_sample;
            if (previous_sample < 0) {
                previous_sample = -previous_sample;
            }
            if ((previous_sample / 0x200) > 0) {
                timer_calibrated = 0;
                return timer_calibrated;
            }
        }
        previous_sample = timer_sample;
    }
    total_samples = (total_samples / 4) - f_9f64();
    if (total_samples < 0) {
        total_samples = -total_samples;
    }
    if (((total_samples * 4) / 0x200) > 0) {
        timer_calibrated = 0;
        return timer_calibrated;
    }
    timer_calibrated = -1;
    return timer_calibrated;
}

/* Timer routine. */

void install_timer_irq(void)
{
    g_7586 = 8;
    g_7585 = g_75a8;
    f_d656(g_756f, (int)reset_timer_counter);
}

/* Timer routine. */

void reset_timer_counter(void)
{
    _disable();
    f_d7b8(g_756f);
    set_pit_channel0_reload(0);
    _enable();
    timer_delta = 0xffff;
    pit_rollover_value = timer_delta;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл start_timer                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

int start_timer(int timer_options)
{
    if (!timer_options) {
        timer_options = g_7588;
        if (!timer_options) {
            return 0;
        }
    }
    if (timer_calibrated == -1 && *(short *)g_756f != -1) {
        timer_delta = f_9f64();
        if (timer_delta > 0x61a8 || timer_delta < 0x2710) {
            timer_calibrated = 0;
            return 0;
        }
        g_7588 = timer_options;
        install_timer_irq();
        pit_rollover_value = timer_delta - 0x100;
        clear_timer_events();
        timer_enabled09 = 1;
        retrace_count5 = 1;
        _disable();
        g_758c = (int)pit_channel0_interrupt;
        g_7590 = (int)o2_e0;
        g_7594 = (int)o2_103;
        f_da01(g_756f);
        set_pit_channel0_reload(0xffff);
        _enable();
        if ((timer_options & 1) == 1) {
            timer_sync_flag_ptr = (unsigned char *)g_75a4;
        }
        if (g_75c4) {
            return 0x502;
        }
        wait_for_tick(0);
        wait_for_tick(0);
    }
    return 0;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл timer_noop                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void timer_noop(void)
{
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл read_pit_counter                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

short read_pit_counter(void)
{
    unsigned short counter_low, counter_high;
    outp(0x43, 0);
    counter_low = inp(0x40);
    counter_high = inp(0x40);
    pit_tick_accumulator5 = pit_counter_snapshot2v;
    pit_counter_snapshot2v = ((unsigned short)counter_high << 8) + (unsigned short)counter_low;
    if (pit_counter_snapshot2v < pit_tick_accumulator5) {
    } else {
        pit_tick_accumulator5 += pit_rollover_value;
    }
    timer_error_hundredths0z = (pit_tick_accumulator5 - pit_counter_snapshot2v) * 100 / 0x4280;
    return timer_error_hundredths0z;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл wait_for_tick                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

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
    if (*(short *)g_756f == -1) {
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
    g_e1c0 = retrace_count5;
    retrace_count5 = 0;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл wait_for_vsync  wait for vertical retrace                              лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void wait_for_vsync(void)
{
    while ((unsigned char)inp(0x3da) & 8) {
        ++retrace_spin_count8;
    }
    while (!((unsigned char)inp(0x3da) & 8)) {
        ++retrace_spin_count8;
    }
}

/* Timer routine. */

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
/* Watcom -ot sentinel: keep the source byte at file offset 10336 as CP437 block.
               л*/
