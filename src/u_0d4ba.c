/* Short external names preserve object chunking; fields describe timer IRQ hook state. */
int dpmi_entry_offset;
/* TU [0xd4ba, 0xdce0): detect_dpmi_host..install (DPMI / interrupt records); from worker u12 T12.c */
#include <conio.h>
int dpmi_entry_selector;
int dpmi_private_data_paragraphs;

#include <stdlib.h>
#include <string.h>
#include <i86.h>

#define DOS_INTERRUPT 0x21
#define DOS_MULTIPLEX_INTERRUPT 0x2f
#define EMS_INTERRUPT 0x67
#define DPMI_INTERRUPT 0x31
#define DOS_GET_VECTOR_FUNCTION 0x35
#define DOS_SET_VECTOR_FUNCTION 0x25
#define DPMI_INSTALLATION_CHECK_FUNCTION 0x1687
#define DPMI_GET_BASE_VECTORS_FUNCTION 0x400
#define DPMI_GET_DESCRIPTOR_FUNCTION 0x200
#define DPMI_SET_DESCRIPTOR_FUNCTION 0x201
#define DPMI_GET_INTERRUPT_VECTOR_FUNCTION 0x204
#define DPMI_SET_INTERRUPT_VECTOR_FUNCTION 0x205
#define PIC_MASTER_DATA_PORT 0x21
#define PIC_SLAVE_DATA_PORT 0xa1
#define PIC_MASTER_VECTOR_BASE 8
#define PIC_MASTER_VECTOR_END 0x10
#define PIC_SLAVE_VECTOR_END 0x18
#define PIC_SLAVE_VECTOR_BASE 0x70
#define EMS_INTERRUPT_VECTOR_ADDRESS 0x19c
#define EMS_GET_MANAGER_STATUS_FUNCTION 0xde00
#define DPMI_VERSION_THOUSAND 0x3e8
#define DPMI_VERSION_HUNDRED 0x64
#define DPMI_VERSION_TEN 0xa
#define DPMI_SELECTOR_MASK 0xffff
#define REAL_MODE_OFFSET_MASK 0xf
#define DPMI_MAPPING_PREFIX_BYTES 0x2d
#define DPMI_MAPPING_POINTER_BIAS 0xd
#define DPMI_MAPPING_ALLOCATION_SLACK 0x10
#define DPMI_MAPPING_SELECTOR_WORD_OFFSET 7
#define DOS_EXTENDED_REGISTERS_BYTES 12
#define INTERRUPT_HOOK_MAP_PHYSICAL_MEMORY 1
#define INTERRUPT_HOOK_DPMI_VECTOR 2
#define INTERRUPT_HOOK_DOS_VECTOR 4
struct InterruptState {
    short status;
    short state_saved;
    short cleanup_registered;
    unsigned int old_dpmi_offset;
    unsigned short old_dpmi_selector;
    unsigned int old_dos_offset;
    unsigned short old_dos_segment;
    unsigned short descriptor_selector;
    unsigned short descriptor_offset;
    unsigned char interrupt_number;
    unsigned char irq_line;
    unsigned char saved_pic_mask;
    unsigned int hook_flags;
    unsigned int handler_address;
    unsigned int physical_start;
    unsigned int physical_end;
    unsigned int mapping_length;
    int allocated_base;
    unsigned int dpmi_memory_handle;
    char *mapped_address;
};
/* The mapped view overlays the tail of the 57-byte interrupt record. */
struct DpmiMapping {
    unsigned char interrupt_state[DPMI_MAPPING_PREFIX_BYTES];
    char *allocation_base;
    unsigned int dpmi_memory_handle;
    char *mapped_address;
};
struct DpmiDescriptorTail {
    unsigned short selector;
    unsigned short offset;
};
extern unsigned int dpmi_selector_or_failure_marker;
extern unsigned int allocate_dpmi_memory(int);
extern void free_dpmi_memory(unsigned int);
extern int dpmi_err;
/* Assembly byte-copy helper, called with source, destination and byte count. */
extern void f_13889(int, int, int);

/* Timer IRQ record and PIC/DPMI hook fields shared by the timer and audio setup. */
/* _DATA [0x74b4,0x75b0): DPMI handles and four 57-byte interrupt records (split at the field names
   other objects import) */
short dpmi_host_available = 0;
unsigned long dpmi_version_bcd = 0xffffffffUL;
/* Unreferenced words in the recovered DPMI/EMS state block; their roles are unknown. */
unsigned short u_74ba = 0;
short ems_manager_available = 0;
unsigned long ems_manager_handle = 0xffffffffUL;
/* This unreferenced word separates the EMS state from the interrupt records. */
unsigned short u_74c2 = 0;
unsigned char sndirq[22] = {0};
unsigned char sndvec = 0;
short sound_system_irq_line = 0;
int sound_system_hook_flags = 0;
int sound_system_handler_address = 0;
int sound_system_physical_start = 0;
unsigned char sound_system_mapping_state[16] = {0};
int sound_system_mapped_address = 0;
unsigned char scbctx[22] = {0};
unsigned char sound_callback_interrupt_number = 0;
short sound_callback_irq_line = 0;
int sound_callback_hook_flags = 0;
int sound_callback_handler_address = 0;
int sound_callback_physical_start = 0;
unsigned char sound_callback_mapping_state[16] = {0};
int sound_callback_mapped_address = 0;
unsigned char key_irq[22] = {0};
unsigned char keyboard_interrupt_number = 0;
short keyboard_irq_line = 0;
int keyboard_hook_flags = 0;
int keyboard_handler_address = 0;
int keyboard_physical_start = 0;
unsigned char keyboard_mapping_state[16] = {0};
int keyboard_mapped_address = 0;
unsigned char tmr_rec[22] = {0};
unsigned char timer_num = 0;
short pic_mask = 0;
int irq_flags = 0;
int irq_handler = 0;
int irq_phys = 0;
unsigned char irq_map[16] = {0};
int irq_addr = 0;
/* Master PIC vector base returned by DPMI. */
unsigned int picvec = 8;
unsigned int slave_pic_vector_base = PIC_SLAVE_VECTOR_BASE;

int detect_dpmi_host(void) {
    struct SREGS sregs;
    union REGS regs;

    memset(&sregs, 0, DOS_EXTENDED_REGISTERS_BYTES);
    regs.w.ax = DPMI_INSTALLATION_CHECK_FUNCTION;
    int386x(DOS_MULTIPLEX_INTERRUPT, &regs, &regs, &sregs);
    if (regs.w.ax == 0) {
        dpmi_version_bcd = (((regs.w.dx % DPMI_VERSION_THOUSAND) / DPMI_VERSION_HUNDRED) << 8) |
                           (((regs.w.dx % DPMI_VERSION_HUNDRED) / DPMI_VERSION_TEN) << 4) |
                           (regs.w.dx % DPMI_VERSION_TEN);
        dpmi_private_data_paragraphs = regs.w.si;
        dpmi_entry_offset = regs.w.di;
        dpmi_entry_selector = sregs.es;

        regs.w.ax = DPMI_GET_BASE_VECTORS_FUNCTION;
        int386x(DPMI_INTERRUPT, &regs, &regs, &sregs);
        *(int *)&picvec = regs.h.dh;
        *(int *)&slave_pic_vector_base = regs.h.dl;
        return dpmi_host_available = -1;
    }
    return dpmi_host_available = 0;
}

int detect_ems_manager(void) {
    union REGS regs;
    struct SREGS sregs;
    if (*(unsigned int *)EMS_INTERRUPT_VECTOR_ADDRESS != 0) {
        memset(&sregs, 0, DOS_EXTENDED_REGISTERS_BYTES);
        regs.w.ax = EMS_GET_MANAGER_STATUS_FUNCTION;
        int386x(EMS_INTERRUPT, &regs, &regs, &sregs);
        if (regs.h.ah == 0) {
            ems_manager_handle = regs.w.bx;
            return ems_manager_available = -1;
        }
    }
    return ems_manager_available = 0;
}

void save_irq(struct InterruptState *record, int cleanup_handler) {
    union REGS regs;
    struct SREGS sregs;
    memset(&sregs, 0, DOS_EXTENDED_REGISTERS_BYTES);
    if (record->state_saved == -1)
        return;
    if (record->hook_flags & INTERRUPT_HOOK_DPMI_VECTOR) {
        regs.x.eax = DPMI_GET_INTERRUPT_VECTOR_FUNCTION;
        regs.h.bl = record->interrupt_number;
        int386x(DPMI_INTERRUPT, &regs, &regs, &sregs);
        record->old_dpmi_selector = regs.x.ecx;
        record->old_dpmi_offset = regs.x.edx;
    }
    if (record->hook_flags & INTERRUPT_HOOK_DOS_VECTOR) {
        regs.x.eax = record->interrupt_number;
        regs.h.ah = DOS_GET_VECTOR_FUNCTION;
        int386x(DOS_INTERRUPT, &regs, &regs, &sregs);
        record->old_dos_segment = sregs.es;
        record->old_dos_offset = regs.x.ebx;
    }
    if ((record->hook_flags & INTERRUPT_HOOK_MAP_PHYSICAL_MEMORY) == INTERRUPT_HOOK_MAP_PHYSICAL_MEMORY) {
        regs.x.eax = DPMI_GET_DESCRIPTOR_FUNCTION;
        regs.h.bl = record->interrupt_number;
        int386(DPMI_INTERRUPT, &regs, &regs);
        record->descriptor_offset = regs.w.cx;
        record->descriptor_selector = regs.w.dx;
    }
    if (record->irq_line < PIC_MASTER_VECTOR_END)
        record->saved_pic_mask = inp(PIC_MASTER_DATA_PORT);
    else if (record->irq_line < PIC_SLAVE_VECTOR_END)
        record->saved_pic_mask = inp(PIC_SLAVE_DATA_PORT);
    if (record->cleanup_registered != -1) {
        if (cleanup_handler)
            atexit((void (*)(void))cleanup_handler);
        record->cleanup_registered = -1;
    }
    record->state_saved = -1;
}

void restore(struct InterruptState *record) {
    struct SREGS sregs;
    union REGS regs;

    memset(&sregs, 0, DOS_EXTENDED_REGISTERS_BYTES);
    if (record->state_saved != -1)
        return;
        if (record->irq_line < PIC_MASTER_VECTOR_END)
            outp(PIC_MASTER_DATA_PORT, inp(PIC_MASTER_DATA_PORT) | (1 << (record->irq_line - PIC_MASTER_VECTOR_BASE)));
        else if (record->irq_line < PIC_SLAVE_VECTOR_END)
            outp(PIC_SLAVE_DATA_PORT, inp(PIC_SLAVE_DATA_PORT) | (1 << (record->irq_line - PIC_MASTER_VECTOR_END)));

        if (record->hook_flags & INTERRUPT_HOOK_DPMI_VECTOR) {
            regs.x.eax = DPMI_SET_INTERRUPT_VECTOR_FUNCTION;
            regs.h.bl = record->interrupt_number;
            regs.w.cx = record->old_dpmi_selector;
            regs.x.edx = record->old_dpmi_offset;
            int386x(DPMI_INTERRUPT, &regs, &regs, &sregs);
        }
        if (record->hook_flags & INTERRUPT_HOOK_DOS_VECTOR) {
            regs.x.eax = record->interrupt_number;
            regs.h.ah = DOS_SET_VECTOR_FUNCTION;
            sregs.ds = record->old_dos_segment;
            regs.x.edx = record->old_dos_offset;
            int386x(DOS_INTERRUPT, &regs, &regs, &sregs);
        }
        if ((record->hook_flags & INTERRUPT_HOOK_MAP_PHYSICAL_MEMORY) == INTERRUPT_HOOK_MAP_PHYSICAL_MEMORY) {
            regs.x.eax = DPMI_SET_DESCRIPTOR_FUNCTION;
            regs.h.bl = record->interrupt_number;
            regs.x.ecx = record->descriptor_offset;
            regs.x.edx = record->descriptor_selector;
            int386(DPMI_INTERRUPT, &regs, &regs);
        }

        if (record->irq_line < PIC_MASTER_VECTOR_END)
            outp(PIC_MASTER_DATA_PORT, (inp(PIC_MASTER_DATA_PORT) & ~(1 << (record->irq_line - PIC_MASTER_VECTOR_BASE))) |
                 (record->saved_pic_mask & (1 << (record->irq_line - PIC_MASTER_VECTOR_BASE))));
        else if (record->irq_line < PIC_SLAVE_VECTOR_END)
            outp(PIC_SLAVE_DATA_PORT, (inp(PIC_SLAVE_DATA_PORT) & ~(1 << (record->irq_line - PIC_MASTER_VECTOR_END))) |
                 (record->saved_pic_mask & (1 << (record->irq_line - PIC_MASTER_VECTOR_END))));

        free_dpmi_memory(record->dpmi_memory_handle);
        record->dpmi_memory_handle = 0;
        if (record->status == -1)
            record->status = 1;
        record->state_saved = 1;
}

int install(struct InterruptState *record) {
    int memory_word_address;
    union REGS regs;
    struct SREGS sregs;

    memset(&sregs, 0, DOS_EXTENDED_REGISTERS_BYTES);
    if (record->status != -1) {
        if (record->irq_line < PIC_MASTER_VECTOR_END)
            outp(PIC_MASTER_DATA_PORT, inp(PIC_MASTER_DATA_PORT) | (1 << (record->irq_line - PIC_MASTER_VECTOR_BASE)));
        else if (record->irq_line < PIC_SLAVE_VECTOR_END)
            outp(PIC_SLAVE_DATA_PORT, inp(PIC_SLAVE_DATA_PORT) | (1 << (record->irq_line - PIC_MASTER_VECTOR_END)));

        if (record->hook_flags & INTERRUPT_HOOK_DPMI_VECTOR) {
            regs.x.eax = DPMI_SET_INTERRUPT_VECTOR_FUNCTION;
            regs.h.bl = record->interrupt_number;
            regs.w.cx = FP_SEG((void (__far *)(void))(void (__near *)(void))record->handler_address);
            regs.x.edx = record->handler_address;
            int386x(DPMI_INTERRUPT, &regs, &regs, &sregs);
            record->status = -1;
        }
        if (record->hook_flags & INTERRUPT_HOOK_DOS_VECTOR) {
            regs.x.eax = record->interrupt_number;
            regs.h.ah = DOS_SET_VECTOR_FUNCTION;
            sregs.ds = FP_SEG((void (__far *)(void))(void (__near *)(void))record->handler_address);
            regs.x.edx = record->handler_address;
            int386x(DOS_INTERRUPT, &regs, &regs, &sregs);
            record->status = -1;
        }
        if ((record->hook_flags & INTERRUPT_HOOK_MAP_PHYSICAL_MEMORY) == INTERRUPT_HOOK_MAP_PHYSICAL_MEMORY) {
            record->mapping_length = record->physical_end - record->physical_start;
            if (record->allocated_base = allocate_dpmi_memory(record->mapping_length + DPMI_MAPPING_ALLOCATION_SLACK)) {
                record->dpmi_memory_handle = dpmi_selector_or_failure_marker;
                ((struct DpmiMapping *)record)->mapped_address =
                    ((struct DpmiMapping *)record)->allocation_base + DPMI_MAPPING_POINTER_BIAS;
                f_13889(record->physical_start, record->allocated_base, record->mapping_length);
                memory_word_address = record->allocated_base + DPMI_MAPPING_SELECTOR_WORD_OFFSET;
                *(unsigned short *)memory_word_address = record->allocated_base >> 4;
                memory_word_address = record->allocated_base + record->mapping_length - 5;
                ((struct DpmiDescriptorTail *)memory_word_address)->selector = record->descriptor_selector;
                ((struct DpmiDescriptorTail *)memory_word_address)->offset = record->descriptor_offset;
                regs.x.eax = DPMI_SET_DESCRIPTOR_FUNCTION;
                regs.h.bl = record->interrupt_number;
                regs.x.ecx = (record->allocated_base >> 4) & DPMI_SELECTOR_MASK;
                regs.x.edx = record->allocated_base & REAL_MODE_OFFSET_MASK;
                int386(DPMI_INTERRUPT, &regs, &regs);
            }
            record->status = -1;
        } else {
            record->dpmi_memory_handle = 0;
            dpmi_err = 0;
        }

        if (record->irq_line < PIC_MASTER_VECTOR_END)
            outp(PIC_MASTER_DATA_PORT, inp(PIC_MASTER_DATA_PORT) & ~(1 << (record->irq_line - PIC_MASTER_VECTOR_BASE)));
        else if (record->irq_line < PIC_SLAVE_VECTOR_END)
            outp(PIC_SLAVE_DATA_PORT, inp(PIC_SLAVE_DATA_PORT) & ~(1 << (record->irq_line - PIC_MASTER_VECTOR_END)));
        return 0;
    }
    return -1;
}
