/* pic.c - 8259A master/slave pair, protected/real-mode vector tables, interrupt delivery.
 *
 * Delivery model (docs/port/architecture.md, "Interrupts"):
 *   - devices call vpic_raise_irq() from any thread (timer thread, audio thread, SDL main);
 *   - the IRQ thread (KE_IRQ=async, default) suspends the game thread; if the game thread is
 *     executing game code (inside the exe's .text, not in a DLL) or is at the explicit
 *     scheduler wait point, has IF=1, is not inside a vhw service and no handler is running,
 *     the handler is called on the IRQ thread while the game thread stays frozen, then resumes;
 *   - otherwise the interrupt stays pending and is delivered synchronously on the game thread
 *     at the end of the next vhw service (vhw_leave) or on STI (_enable).
 * Handlers are plain cdecl functions (Watcom __interrupt dropped): the PM vector offset is
 * the host address of the game's handler. Vectors the game did not hook run the built-in
 * BIOS/DOS-extender default for that IRQ (tick count, BIOS keyboard, EOI).
 * Limitations: no nested interrupts (one handler at a time), fully nested priority mode only,
 * edge-triggered IRR (a raise stays pending until acknowledged).
 */
#include <setjmp.h>
#include <string.h>
#include "../platform/ke_platform.h"
#include "vhw.h"
#include "../include/ke_port.h"

typedef struct Pic {
    uint8_t irr, isr, imr;
    uint8_t read_isr;      /* OCW3: next read of the command port returns ISR (else IRR) */
    uint8_t base;          /* vector base                                                */
    uint8_t icw_step;      /* initialization sequence progress                           */
} Pic;

static Pic pics[2];
static KeMutex pic_lock;
static struct { uint16_t sel; uint32_t off; } pm_vectors[256];
static struct { uint16_t seg, off; } rm_vectors[256];
static KeEvent *irq_event;
static KeThread *irq_thread;
static ke_atomic_t irq_stop;
static uintptr_t text_lo, text_hi;          /* game thread interruptible PC range      */
static uint64_t irq_edge_ns[16];            /* original device edge for pending IRQs     */
static jmp_buf isr_abandon;                 /* IRQ thread: leave a handler that exited  */
static ke_atomic_t isr_exit_code = -1;
static long stats_async, stats_sync, stats_blocked;
/* Lockstep oracle (port/oracle/lockstep.c): original KE.EXE handlers end in IRETD and were
 * installed with the host's real code selector (FP_SEG of a near function = CS). */
void (*vpic_isr_invoker)(uint32_t offset);
uint16_t vpic_extra_code_selector;

/* ---- priority resolution ----------------------------------------------------------- */
static int pic_pick(Pic *p)
{
    uint8_t req = p->irr & (uint8_t)~p->imr;
    int i;
    for (i = 0; i < 8; i++) {
        uint8_t bit = (uint8_t)(1u << i);
        if (p->isr & bit)
            return -1;         /* equal or higher priority in service */
        if (req & bit)
            return i;
    }
    return -1;
}

/* Returns the IRQ number (0..15) that would be acknowledged now, or -1. Lock held. */
static int pick_locked(void)
{
    int m = pic_pick(&pics[0]);
    if (m == 2) {              /* cascade */
        int s = pic_pick(&pics[1]);
        return s >= 0 ? 8 + s : -1;
    }
    return m;
}

static void update_cascade_locked(void)
{
    if (pics[1].irr & (uint8_t)~pics[1].imr)
        pics[0].irr |= 4;
    else
        pics[0].irr &= (uint8_t)~4;
}

/* A clock hold represents host delivery latency only. Guest CLI/PIC masking leaves the
 * physical PIT and raster running, so release the hold as soon as IRQ0 cannot be delivered. */
static void release_irq0_clock_if_unavailable_locked(void)
{
    if (!vcpu_if_flag || pick_locked() != 0)
        vhw_clock_irq0_release();
}

void vpic_raise_irq(int irq)
{
    if (irq < 0 || irq >= 16)
        return;
    ke_mutex_lock(&pic_lock);
    if (irq < 8) {
        if (!(pics[0].irr & (uint8_t)(1u << irq)))
            irq_edge_ns[irq] = 0;
        pics[0].irr |= (uint8_t)(1u << irq);
    } else {
        if (!(pics[1].irr & (uint8_t)(1u << (irq - 8))))
            irq_edge_ns[irq] = 0;
        pics[1].irr |= (uint8_t)(1u << (irq - 8));
        update_cascade_locked();
    }
    ke_mutex_unlock(&pic_lock);
    if (irq_event)
        ke_event_set(irq_event);
}

void vpic_raise_irq_at(int irq, uint64_t edge_ns)
{
    if (irq < 0 || irq >= 16)
        return;
    ke_mutex_lock(&pic_lock);
    if (irq < 8) {
        if (!(pics[0].irr & (uint8_t)(1u << irq)))
            irq_edge_ns[irq] = edge_ns;
        pics[0].irr |= (uint8_t)(1u << irq);
    } else {
        if (!(pics[1].irr & (uint8_t)(1u << (irq - 8))))
            irq_edge_ns[irq] = edge_ns;
        pics[1].irr |= (uint8_t)(1u << (irq - 8));
        update_cascade_locked();
    }
    if (irq == 0 && edge_ns && vcpu_if_flag && pick_locked() == 0)
        vhw_clock_irq0_pending(edge_ns);
    ke_mutex_unlock(&pic_lock);
    if (irq_event)
        ke_event_set(irq_event);
}

void vpic_lower_irq(int irq)
{
    if (irq < 0 || irq >= 16)
        return;
    ke_mutex_lock(&pic_lock);
    if (irq < 8)
        pics[0].irr &= (uint8_t)~(1u << irq);
    else {
        pics[1].irr &= (uint8_t)~(1u << (irq - 8));
        update_cascade_locked();
    }
    irq_edge_ns[irq] = 0;
    if (irq == 0)
        vhw_clock_irq0_release();
    ke_mutex_unlock(&pic_lock);
}

int vpic_has_deliverable(void)
{
    int r;
    ke_mutex_lock(&pic_lock);
    r = pick_locked();
    ke_mutex_unlock(&pic_lock);
    return r >= 0;
}

int vpic_vector_base(int slave) { return pics[slave ? 1 : 0].base; }

/* ---- vector tables ------------------------------------------------------------------ */
void vpic_set_pm_vector(int v, uint16_t sel, uint32_t off)
{
    pm_vectors[v & 0xff].sel = sel;
    pm_vectors[v & 0xff].off = off;
    ke_log(KE_LOG_DEBUG, "pic", "PM vector %02Xh <- %04X:%08X", v & 0xff, sel, off);
}
void vpic_get_pm_vector(int v, uint16_t *sel, uint32_t *off)
{
    *sel = pm_vectors[v & 0xff].sel;
    *off = pm_vectors[v & 0xff].off;
}
void vpic_set_rm_vector(int v, uint16_t seg, uint16_t off)
{
    rm_vectors[v & 0xff].seg = seg;
    rm_vectors[v & 0xff].off = off;
    memcpy(ke_lowmem_shadow + 4 * (v & 0xff), &off, 2);
    memcpy(ke_lowmem_shadow + 4 * (v & 0xff) + 2, &seg, 2);
}
void vpic_get_rm_vector(int v, uint16_t *seg, uint16_t *off)
{
    *seg = rm_vectors[v & 0xff].seg;
    *off = rm_vectors[v & 0xff].off;
}

/* ---- ports 20h/21h, A0h/A1h --------------------------------------------------------- */
static void eoi_locked(Pic *p)
{
    int i;
    for (i = 0; i < 8; i++)
        if (p->isr & (1u << i)) {
            p->isr &= (uint8_t)~(1u << i);
            return;
        }
}

static uint32_t pic_in(void *ctx, uint16_t port, int size)
{
    Pic *p = &pics[(port & 0x80) ? 1 : 0];
    uint32_t v;
    (void)ctx; (void)size;
    ke_mutex_lock(&pic_lock);
    v = (port & 1) ? p->imr : (p->read_isr ? p->isr : p->irr);
    ke_mutex_unlock(&pic_lock);
    return v;
}

static void pic_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    Pic *p = &pics[(port & 0x80) ? 1 : 0];
    int changed_mask = 0;
    (void)ctx; (void)size;
    value &= 0xff;
    ke_mutex_lock(&pic_lock);
    if (port & 1) {
        if (p->icw_step == 1) {           /* ICW2: vector base */
            p->base = (uint8_t)(value & 0xf8);
            p->icw_step = 2;
        } else if (p->icw_step == 2) {    /* ICW3 */
            p->icw_step = 3;
        } else if (p->icw_step == 3) {    /* ICW4 */
            p->icw_step = 0;
        } else {                          /* OCW1: mask */
            p->imr = (uint8_t)value;
            changed_mask = 1;
        }
    } else if (value & 0x10) {            /* ICW1 */
        p->icw_step = 1;
        p->imr = 0;
        p->isr = 0;
        p->read_isr = 0;
    } else if ((value & 0x18) == 0x08) {  /* OCW3 */
        if (value & 2)
            p->read_isr = (uint8_t)(value & 1);
    } else if ((value & 0xe0) == 0x20) {  /* non-specific EOI */
        eoi_locked(p);
    } else if ((value & 0xe0) == 0x60) {  /* specific EOI */
        p->isr &= (uint8_t)~(1u << (value & 7));
    }
    update_cascade_locked();
    release_irq0_clock_if_unavailable_locked();
    ke_mutex_unlock(&pic_lock);
    if (irq_event && (changed_mask || !(port & 1)))
        ke_event_set(irq_event);
}

/* ---- default (unhooked) handlers ---------------------------------------------------- */
static void default_handler(int irq)
{
    if (irq == 0) {
        uint32_t ticks;
        memcpy(&ticks, ke_lowmem_shadow + 0x46c, 4);
        ticks++;
        memcpy(ke_lowmem_shadow + 0x46c, &ticks, 4);
    } else if (irq == 1) {
        vkbd_bios_irq1();
    } else {
        ke_log_once("pic.spurious", KE_LOG_DEBUG, "pic", "IRQ %d without a game handler", irq);
    }
    vhw_port_out(0x20, 0x20, 1);
    if (irq >= 8)
        vhw_port_out(0xa0, 0x20, 1);
}

/* Acknowledge the best pending IRQ and run its handler on the calling thread. */
static int deliver_one(void)
{
    int irq, vector;
    int clock_scoped;
    uint32_t off;
    uint64_t edge_ns;
    ke_atomic_value saved_if;
    uint32_t saved_inp = vhw_last_inp_value;   /* handler registers are restored by IRET */
    ke_mutex_lock(&pic_lock);
    irq = pick_locked();
    if (irq < 0) {
        ke_mutex_unlock(&pic_lock);
        return 0;
    }
    edge_ns = irq_edge_ns[irq];
    irq_edge_ns[irq] = 0;
    if (irq >= 8) {
        pics[1].irr &= (uint8_t)~(1u << (irq - 8));
        pics[1].isr |= (uint8_t)(1u << (irq - 8));
        pics[0].isr |= 4;
        update_cascade_locked();
        vector = pics[1].base + (irq - 8);
    } else {
        pics[0].irr &= (uint8_t)~(1u << irq);
        pics[0].isr |= (uint8_t)(1u << irq);
        vector = pics[0].base + irq;
    }
    ke_atomic_exchange(&vhw_in_isr, 1);
    ke_mutex_unlock(&pic_lock);

    saved_if = ke_atomic_exchange(&vcpu_if_flag, 0);   /* INT clears IF, IRET restores */
    clock_scoped = irq == 0 && edge_ns && vhw_clock_irq0_held();
    if (clock_scoped)
        vhw_clock_irq0_enter(edge_ns);
    off = pm_vectors[vector].off;
    /* A vector counts as hooked only when it holds a flat-model code address; saved and
     * restored BIOS defaults (F000:xxxx, 0:0) run the built-in default handler. */
    if (off && (pm_vectors[vector].sel == KE_FLAT_SELECTOR ||
                (vpic_extra_code_selector && pm_vectors[vector].sel == vpic_extra_code_selector))) {
        if (vpic_isr_invoker)
            vpic_isr_invoker(off);
        else
            ((void (*)(void))(uintptr_t)off)();
    }
    else
        default_handler(irq);
    if (clock_scoped)
        vhw_clock_irq0_leave();
    ke_atomic_exchange(&vcpu_if_flag, saved_if);
    ke_atomic_exchange(&vhw_in_isr, 0);
    vhw_last_inp_value = saved_inp;
    return 1;
}

int vpic_deliver_pending(void)
{
    int n = 0;
    while (vcpu_if_flag && !vhw_in_isr && n < 16 && deliver_one()) {
        n++;
        stats_sync++;
    }
    ke_mutex_lock(&pic_lock);
    release_irq0_clock_if_unavailable_locked();
    ke_mutex_unlock(&pic_lock);
    return n;
}

static void release_irq0_clock_if_unavailable(void)
{
    ke_mutex_lock(&pic_lock);
    release_irq0_clock_if_unavailable_locked();
    ke_mutex_unlock(&pic_lock);
}

/* ---- asynchronous delivery thread --------------------------------------------------- */
static void find_text_range(void)
{
    ke_platform_code_range(&text_lo, &text_hi);
}

/* ke_exit() inside a handler on the IRQ thread: redirect the frozen game thread into
 * ke_exit(code) and abandon the handler. */
void vpic_isr_exit_redirect(int code)
{
    if (!vhw_on_irq_thread() || ke_thread_interrupt_runs_on_target())
        return;
    ke_atomic_exchange(&isr_exit_code, code);
    longjmp(isr_abandon, 1);
}

/* ke_exit() inside a handler delivered asynchronously on the game thread itself (POSIX):
 * the game thread performs the exit directly; undo the interrupt entry first, as the Win32
 * redirect does, and release the interrupter. */
void vpic_isr_exit_on_game_thread(void)
{
    if (!vhw_in_async_isr_context())
        return;
    ke_atomic_exchange(&vcpu_if_flag, 1);
    ke_atomic_exchange(&vhw_in_isr, 0);
    vhw_clock_irq0_leave();
    vhw_clock_irq0_release();
    vhw_set_async_isr_context(0);
    ke_thread_interrupt_leaving();
}

static void redirect_exit_target(int code) { ke_exit(code); }

/* Runs while the game thread is stopped at an instruction boundary: on the IRQ thread with
 * the game thread suspended (Win32), or on the game thread inside the interrupt signal
 * (POSIX). Delivers one interrupt if the game is interruptible at `pc`. */
static int async_deliver(uintptr_t pc, void *arg, KeRedirect *redirect)
{
    int delivered = 0;
    (void)arg;
    if (pc != UINTPTR_MAX && vcpu_if_flag && !vhw_game_depth && !vhw_in_isr &&
        ((pc >= text_lo && pc < text_hi) || vhw_cpu_poll_waiting)) {
        if (ke_thread_interrupt_runs_on_target()) {
            vhw_set_async_isr_context(1);
            delivered = deliver_one();
            vhw_set_async_isr_context(0);
        } else if (setjmp(isr_abandon) == 0) {
            delivered = deliver_one();
        } else {
            /* the handler called exit(): the game thread performs it */
            ke_atomic_exchange(&vcpu_if_flag, 1);
            ke_atomic_exchange(&vhw_in_isr, 0);
            vhw_clock_irq0_leave();
            vhw_clock_irq0_release();
            redirect->fn = redirect_exit_target;
            redirect->arg = (int)isr_exit_code;
            return -2;
        }
        stats_async += delivered;
    } else {
        stats_blocked++;
    }
    if (delivered) {
        release_irq0_clock_if_unavailable();
    }
    return delivered;
}

static unsigned irq_thread_main(void *unused)
{
    KeThread *game = ke_game_thread_handle();
    (void)unused;
    vhw_bind_irq_thread();
    ke_thread_set_time_critical();
    while (!irq_stop) {
        int pending = vpic_has_deliverable();
        ke_event_wait_ms(irq_event, pending ? 1 : 20);
        if (irq_stop || ke_game_thread_finished())
            break;
        int retries = 0;
        while (vpic_has_deliverable() && !irq_stop) {
            int delivered;
            if (!vcpu_if_flag || vhw_game_depth || vhw_in_isr) {
                stats_blocked++;
                if (!vcpu_if_flag)
                    release_irq0_clock_if_unavailable();
                break;             /* sync delivery at vhw_leave/STI will handle it */
            }
            delivered = ke_thread_interrupt(game, async_deliver, NULL);
            if (delivered == -2)
                return 0;          /* the game thread was redirected into exit() */
            if (delivered < 0)
                break;
            if (!delivered) {
                /* Game thread momentarily in a DLL or a vhw service: retry for ~2 ms in
                 * 20 us steps before falling back to the event wait (the sync path at
                 * vhw_leave delivers anyway if the game is inside the virtual PC). */
                uint64_t until = ke_now_ns() + 20000;
                if (++retries > 100)
                    break;
                while (ke_now_ns() < until)
                    ke_cpu_relax();
                continue;
            }
            retries = 0;
        }
    }
    return 0;
}

void virq_thread_start(void)
{
    find_text_range();
    if (!ke_config.irq_async) {
        ke_log(KE_LOG_INFO, "pic", "interrupt delivery: synchronous only (KE_IRQ=sync)");
        return;
    }
    irq_thread = ke_thread_create(irq_thread_main, NULL, 256u << 10, 0);
    ke_log(KE_LOG_INFO, "pic", "interrupt delivery: asynchronous IRQ thread + sync at vhw_leave; "
           "interruptible code %08lX..%08lX", (unsigned long)text_lo, (unsigned long)text_hi);
}

void virq_thread_stop(void)
{
    ke_atomic_exchange(&irq_stop, 1);
    if (irq_event)
        ke_event_set(irq_event);
    if (irq_thread) {
        ke_thread_join_ms(irq_thread, 2000);
        ke_thread_close(irq_thread);
        irq_thread = NULL;
    }
    ke_log(KE_LOG_INFO, "pic", "interrupts delivered: async=%ld sync=%ld (blocked attempts %ld)",
           stats_async, stats_sync, stats_blocked);
}

/* Side-effect-free PIC state for lockstep dumps: IRR/ISR/IMR/base per controller + IF. */
void vpic_debug_state(uint8_t out[10])
{
    ke_mutex_lock(&pic_lock);
    out[0] = pics[0].irr; out[1] = pics[0].isr; out[2] = pics[0].imr; out[3] = pics[0].base;
    out[4] = pics[1].irr; out[5] = pics[1].isr; out[6] = pics[1].imr; out[7] = pics[1].base;
    ke_mutex_unlock(&pic_lock);
    out[8] = (uint8_t)vcpu_if_flag;
    out[9] = (uint8_t)vhw_in_isr;
}

void vpic_init(void)
{
    int v;
    ke_mutex_init(&pic_lock);
    memset(irq_edge_ns, 0, sizeof irq_edge_ns);
    irq_event = ke_event_create();
    memset(pics, 0, sizeof pics);
    pics[0].base = 0x08;       /* as reported by DOS/4GW (DPMI 0400h DH/DL) */
    pics[1].base = 0x70;
    pics[0].imr = 0xb8;        /* typical BIOS state: IRQ0,1,2,6 enabled   */
    pics[1].imr = 0x9d;
    for (v = 0; v < 256; v++)
        vpic_set_rm_vector(v, KE_BIOS_ROM_SEGMENT, KE_BIOS_IRET_OFFSET);
    vhw_register_ports(0x20, 0x21, pic_in, pic_out, NULL, "pic-master");
    vhw_register_ports(0xa0, 0xa1, pic_in, pic_out, NULL, "pic-slave");
}
