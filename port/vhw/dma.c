/* dma.c - 8237A DMA controller, 8-bit channels 0-3 (ports 00h-0Fh, page registers 81h-87h).
 *
 * Physical address == linear address: DMA buffers are DOS memory below 1 MiB, identity
 * mapped by lowmem.c, so a transfer reads host memory at (page << 16) | address.
 * Only memory->device (read) transfers are consumed (the Sound Blaster pulls them).
 */
#include <string.h>
#include <windows.h>
#include "vhw.h"
#include "../include/ke_port.h"

typedef struct DmaChannel {
    uint16_t base_addr, base_count, cur_addr, cur_count;
    uint8_t page, mode, masked, tc;
} DmaChannel;

static DmaChannel dch[4];
static int flipflop;
static CRITICAL_SECTION dma_lock;
static const uint16_t page_ports[4] = {0x87, 0x83, 0x81, 0x82};

static uint32_t dma_in(void *ctx, uint16_t port, int size)
{
    uint32_t v = 0xff;
    (void)ctx; (void)size;
    EnterCriticalSection(&dma_lock);
    if (port < 8) {
        DmaChannel *c = &dch[port >> 1];
        uint16_t w = (port & 1) ? c->cur_count : c->cur_addr;
        v = flipflop ? (w >> 8) : (w & 0xff);
        flipflop ^= 1;
    } else if (port == 0x08) {             /* status: TC bits 0-3 (cleared on read) */
        int i;
        v = 0;
        for (i = 0; i < 4; i++) {
            if (dch[i].tc) v |= 1u << i;
            dch[i].tc = 0;
        }
    } else {
        int i;
        for (i = 0; i < 4; i++)
            if (port == page_ports[i])
                v = dch[i].page;
    }
    LeaveCriticalSection(&dma_lock);
    return v;
}

static void dma_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    int i;
    (void)ctx; (void)size;
    value &= 0xff;
    EnterCriticalSection(&dma_lock);
    if (port < 8) {
        DmaChannel *c = &dch[port >> 1];
        uint16_t *base = (port & 1) ? &c->base_count : &c->base_addr;
        uint16_t *cur = (port & 1) ? &c->cur_count : &c->cur_addr;
        if (!flipflop)
            *base = (uint16_t)((*base & 0xff00) | value);
        else
            *base = (uint16_t)((*base & 0x00ff) | (value << 8));
        *cur = *base;
        flipflop ^= 1;
    } else if (port == 0x0a) {             /* single mask */
        dch[value & 3].masked = (uint8_t)((value >> 2) & 1);
    } else if (port == 0x0b) {             /* mode */
        dch[value & 3].mode = (uint8_t)value;
    } else if (port == 0x0c) {             /* clear flip-flop */
        flipflop = 0;
    } else if (port == 0x0d) {             /* master clear */
        for (i = 0; i < 4; i++)
            dch[i].masked = 1;
        flipflop = 0;
    } else if (port == 0x0f) {             /* write all masks */
        for (i = 0; i < 4; i++)
            dch[i].masked = (uint8_t)((value >> i) & 1);
    } else {
        for (i = 0; i < 4; i++)
            if (port == page_ports[i])
                dch[i].page = (uint8_t)value;
    }
    LeaveCriticalSection(&dma_lock);
}

int vdma_read(int channel, uint8_t *dst, int len, int *terminal)
{
    DmaChannel *c = &dch[channel & 3];
    int n = 0;
    *terminal = 0;
    EnterCriticalSection(&dma_lock);
    if (!c->masked) {
        while (n < len) {
            uint32_t linear = ((uint32_t)c->page << 16) | c->cur_addr;
            dst[n++] = (linear >= LOWMEM_BASE && linear < LOWMEM_END) ? *(uint8_t *)(uintptr_t)linear
                                                                 : 0x80;
            c->cur_addr++;
            if (c->cur_count-- == 0) {     /* count register holds length-1 */
                *terminal = 1;
                c->tc = 1;
                if (c->mode & 0x10) {      /* auto-initialize */
                    c->cur_addr = c->base_addr;
                    c->cur_count = c->base_count;
                } else {
                    c->masked = 1;
                }
                break;
            }
        }
    }
    LeaveCriticalSection(&dma_lock);
    return n;
}

void vdma_init(void)
{
    int i;
    InitializeCriticalSection(&dma_lock);
    for (i = 0; i < 4; i++)
        dch[i].masked = 1;
    vhw_register_ports(0x00, 0x0f, dma_in, dma_out, NULL, "dma8");
    vhw_register_ports(0x81, 0x83, dma_in, dma_out, NULL, "dma-page");
    vhw_register_ports(0x87, 0x87, dma_in, dma_out, NULL, "dma-page");
}
