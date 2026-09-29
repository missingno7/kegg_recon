/* vhw.c - virtual PC assembly: device initialisation order and shutdown. */
#include "../platform/ke_platform.h"
#include "vhw.h"
#include "../include/ke_port.h"

void vpit_start(void);

int vhw_init(void)
{
    if (!vhw_lockstep)
        ke_timer_resolution_begin(); /* 1 ms scheduler granularity for device threads */
    vhw_fault_init();
    if (lowmem_init() != 0)
        return -1;
    vpic_init();
    vpit_init();
    vga_init();
    vkbd_init();
    vmouse_init();
    vjoy_init();
    vdma_init();
    /* vsb_init() runs after SDL_Init (main_sdl.c): it opens an SDL audio stream */
    return 0;
}

void vhw_start_devices(void)
{
    if (vhw_lockstep)
        return;                /* lockstep: PIT edges and IRQs are driven by the clock */
    virq_thread_start();
    vpit_start();
}

void vhw_shutdown(void)
{
    vpit_shutdown();
    virq_thread_stop();
    vsb_shutdown();
}
