/* m_11494_11530.c - literal C translation of asm/m_11494_11530.asm.
 * The parameter block and register writes mirror the historical 8237 sequence.
 */
#include <stdint.h>

extern int outp(int port, int value);
extern int inp(int port);

unsigned int sound_dma_buffer_address;
short sound_dma_transfer_count;
unsigned char sound_dma_mode_bits;
unsigned char sound_dma_channel;

#define DMA_MASK_PORT       0x0a
#define DMA_MODE_PORT       0x0b
#define DMA_CLEAR_FF_PORT   0x0c
#define DMA_CHANNEL_MASK    0x04
#define DMA_PAGE_LOOKUP     0x82818387u

void mask_sound_dma_channel(void)
{
    unsigned char al = (unsigned char)(sound_dma_channel | DMA_CHANNEL_MASK);
    outp(DMA_MASK_PORT, al);
}

void sound_dma_mask_entry(void)
{
    mask_sound_dma_channel();
}

void program_sound_dma_channel(void)
{
    unsigned char cl = sound_dma_channel;
    unsigned char al = (unsigned char)(cl | DMA_CHANNEL_MASK);
    uint16_t dx;
    uint32_t edx;
    uint32_t eax = sound_dma_buffer_address;
    uint16_t count;

    outp(DMA_MASK_PORT, al);
    outp(DMA_CLEAR_FF_PORT, al);
    al = (unsigned char)(cl | sound_dma_mode_bits);
    outp(DMA_MODE_PORT, al);

    dx = (uint16_t)((uint16_t)cl * 2u);
    outp(dx, (unsigned char)eax);
    outp(dx, (unsigned char)(eax >> 8));
    dx = (uint16_t)(dx + 1u);
    count = (uint16_t)((uint16_t)sound_dma_transfer_count - 1u);
    outp(dx, (unsigned char)count);
    outp(dx, (unsigned char)(count >> 8));

    cl = (unsigned char)(cl << 3);        /* QUIRK: 8-bit shift count and channel rotate */
    edx = DMA_PAGE_LOOKUP >> (cl & 31u);
    dx = (uint16_t)edx;
    dx &= 0x00ffu;
    cl = (unsigned char)(cl >> 3);
    outp(dx, (unsigned char)(eax >> 16));
    outp(DMA_MASK_PORT, cl);
}
