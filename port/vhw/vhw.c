/* vhw.c - virtual PC assembly: device initialisation order and shutdown. */
#include <windows.h>
#include <timeapi.h>
#include "vhw.h"
#include "../include/ke_port.h"

void vpit_start(void);

int vhw_init(void)
{
    timeBeginPeriod(1);        /* 1 ms scheduler granularity for device threads */
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
    virq_thread_start();
    vpit_start();
}

void vhw_shutdown(void)
{
    vpit_shutdown();
    virq_thread_stop();
    vsb_shutdown();
}
