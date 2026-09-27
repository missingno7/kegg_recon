#pragma aux save_interrupt_state "f_d656";
#pragma aux restore_interrupt_state "f_d7b8";
#pragma aux install_interrupt_state "f_da01";
#pragma aux timer_interrupt_record "g_756f";
#pragma aux master_pic_vector_base "g_75a8";
#pragma aux dpmi_memory_error "g_75c4";
int dpmi_entry_offset;
/* TU [0xd4ba, 0xdce0): detect_dpmi_host..install_interrupt_state (DPMI / interrupt records); from worker u12 T12.c */
#include <conio.h>
int dpmi_entry_selector;
int dpmi_private_data_paragraphs;

#include <stdlib.h>
#include <string.h>
#include <i86.h>
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
struct DpmiMapping {
    unsigned char interrupt_state[0x2d];
    char *allocation_base;
    unsigned int dpmi_memory_handle;
    char *mapped_address;
};
extern short sound_blaster_base_port;
extern unsigned char g_7db2;
extern unsigned char g_7db3;
extern int saved_sound_mixer_value;
extern int select_sound_blaster_port(void);
extern int detect_sound_blaster_irq(void);
extern int detect_sound_blaster_dma(void);
extern int f_1144d(void);
extern int f_11420(void);
extern int f_113f8(void);
extern int f_11485(void);
extern int sound_irq_test_complete_l;
extern void save_interrupt_state(struct InterruptState *, int);
extern int install_interrupt_state(unsigned char *);
extern void restore_interrupt_state(struct InterruptState *);
extern void __interrupt sound_test_irq_handler(void);
extern int sound_dma_test_result;
extern unsigned int dpmi_linear_address_value;
extern unsigned int g_7db4;
extern short g_7db8;
extern unsigned char g_7dba;
extern unsigned char g_7dbb;
extern unsigned int allocate_dpmi_memory(int);
extern void free_dpmi_memory(unsigned int);
extern void f_11494(void);
extern void f_114a0(void);
extern void f_11377(unsigned int);
extern void copy_ds_to_es(void);
extern void f_113bd(void);
extern unsigned int g_e300;
extern unsigned int g_e304;
extern unsigned int g_1258;
int detect_vga_bios_mode(void);
int detect_xms_driver(void);
int check_ems_manager_signature(void);
extern void f_14197(void *);
extern int dpmi_memory_error;
extern void f_13889(int, int, int);

extern short sound_blaster_detected;
extern unsigned int sound_blaster_mixer_test;
extern unsigned int sound_blaster_dsp_version;
extern unsigned char sound_blaster_irq;
extern unsigned char sound_blaster_dma_channel;
extern unsigned int u_7488;
extern char *sound_blaster_env_name;
extern short dos_version_query_succeeded;
extern unsigned long dos_version_packed;
extern unsigned short u_7496;
extern short vga_bios_mode_supported;
extern unsigned short u_749a;
extern unsigned short u_749c;
extern unsigned long g_749e;
extern unsigned short u_74a2;
extern short xms_driver_available;
extern unsigned long g_74a6;
extern unsigned short u_74aa;
extern short ems_manager_signature_found;
extern unsigned long g_74ae;
extern unsigned short u_74b2;

/* _DATA [0x74b4,0x75b0): DPMI handles and four 57-byte interrupt records (split at the field names
   other objects import) */
short dpmi_host_available = 0;
unsigned long dpmi_version_bcd = 0xffffffffUL;
unsigned short u_74ba = 0;
short ems_manager_available = 0;
unsigned long ems_manager_handle = 0xffffffffUL;
unsigned short u_74c2 = 0;
unsigned char g_74c4[22] = {0};
unsigned char g_74da = 0;
short g_74db = 0;
int g_74dd = 0;
int g_74e1 = 0;
int g_74e5 = 0;
unsigned char g_74e9[16] = {0};
int g_74f9 = 0;
unsigned char g_74fd[22] = {0};
unsigned char g_7513 = 0;
short g_7514 = 0;
int g_7516 = 0;
int g_751a = 0;
int g_751e = 0;
unsigned char g_7522[16] = {0};
int g_7532 = 0;
unsigned char key_irq[22] = {0};
unsigned char g_754c = 0;
short g_754d = 0;
int g_754f = 0;
int g_7553 = 0;
int g_7557 = 0;
unsigned char g_755b[16] = {0};
int g_756b = 0;
unsigned char timer_interrupt_record[22] = {0};
unsigned char g_7585 = 0;
short g_7586 = 0;
int g_7588 = 0;
int g_758c = 0;
int g_7590 = 0;
unsigned char g_7594[16] = {0};
int g_75a4 = 0;
unsigned int master_pic_vector_base = 8;
unsigned int slave_pic_vector_base = 0x70;

int detect_dpmi_host(void) {
    struct SREGS sregs;
    union REGS regs;

    memset(&sregs, 0, 12);
    regs.w.ax = 0x1687;
    int386x(0x2f, &regs, &regs, &sregs);
    if (regs.w.ax == 0) {
        dpmi_version_bcd = (((regs.w.dx % 0x3e8) / 0x64) << 8) |
                           (((regs.w.dx % 0x64) / 0xa) << 4) |
                           (regs.w.dx % 0xa);
        dpmi_private_data_paragraphs = regs.w.si;
        dpmi_entry_offset = regs.w.di;
        dpmi_entry_selector = sregs.es;

        regs.w.ax = 0x400;
        int386x(0x31, &regs, &regs, &sregs);
        *(int *)&master_pic_vector_base = regs.h.dh;
        *(int *)&slave_pic_vector_base = regs.h.dl;
        return dpmi_host_available = -1;
    }
    return dpmi_host_available = 0;
}

int detect_ems_manager(void) {
    union REGS regs;
    struct SREGS sregs;
    if (*(unsigned int *)0x19c != 0) {
        memset(&sregs, 0, 12);
        regs.w.ax = 0xde00;
        int386x(0x67, &regs, &regs, &sregs);
        if (regs.h.ah == 0) {
            ems_manager_handle = regs.w.bx;
            return ems_manager_available = -1;
        }
    }
    return ems_manager_available = 0;
}

void save_interrupt_state(struct InterruptState *record, int cleanup_handler) {
    union REGS regs;
    struct SREGS sregs;
    memset(&sregs, 0, 12);
    if (record->state_saved == -1)
        return;
    if (record->hook_flags & 2) {
        regs.x.eax = 0x204;
        regs.h.bl = record->interrupt_number;
        int386x(0x31, &regs, &regs, &sregs);
        record->old_dpmi_selector = regs.x.ecx;
        record->old_dpmi_offset = regs.x.edx;
    }
    if (record->hook_flags & 4) {
        regs.x.eax = record->interrupt_number;
        regs.h.ah = 0x35;
        int386x(0x21, &regs, &regs, &sregs);
        record->old_dos_segment = sregs.es;
        record->old_dos_offset = regs.x.ebx;
    }
    if ((record->hook_flags & 1) == 1) {
        regs.x.eax = 0x200;
        regs.h.bl = record->interrupt_number;
        int386(0x31, &regs, &regs);
        record->descriptor_offset = regs.w.cx;
        record->descriptor_selector = regs.w.dx;
    }
    if (record->irq_line < 0x10)
        record->saved_pic_mask = inp(0x21);
    else if (record->irq_line < 0x18)
        record->saved_pic_mask = inp(0xa1);
    if (record->cleanup_registered != -1) {
        if (cleanup_handler)
            atexit((void (*)(void))cleanup_handler);
        record->cleanup_registered = -1;
    }
    record->state_saved = -1;
}

void restore_interrupt_state(struct InterruptState *record) {
    struct SREGS sregs;
    union REGS regs;

    memset(&sregs, 0, 0xc);
    if (record->state_saved != -1)
        return;
        if (record->irq_line < 0x10)
            outp(0x21, inp(0x21) | (1 << (record->irq_line - 8)));
        else if (record->irq_line < 0x18)
            outp(0xa1, inp(0xa1) | (1 << (record->irq_line - 0x10)));

        if (record->hook_flags & 2) {
            regs.x.eax = 0x205;
            regs.h.bl = record->interrupt_number;
            regs.w.cx = record->old_dpmi_selector;
            regs.x.edx = record->old_dpmi_offset;
            int386x(0x31, &regs, &regs, &sregs);
        }
        if (record->hook_flags & 4) {
            regs.x.eax = record->interrupt_number;
            regs.h.ah = 0x25;
            sregs.ds = record->old_dos_segment;
            regs.x.edx = record->old_dos_offset;
            int386x(0x21, &regs, &regs, &sregs);
        }
        if ((record->hook_flags & 1) == 1) {
            regs.x.eax = 0x201;
            regs.h.bl = record->interrupt_number;
            regs.x.ecx = record->descriptor_offset;
            regs.x.edx = record->descriptor_selector;
            int386(0x31, &regs, &regs);
        }

        if (record->irq_line < 0x10)
            outp(0x21, (inp(0x21) & ~(1 << (record->irq_line - 8))) |
                 (record->saved_pic_mask & (1 << (record->irq_line - 8))));
        else if (record->irq_line < 0x18)
            outp(0xa1, (inp(0xa1) & ~(1 << (record->irq_line - 0x10))) |
                 (record->saved_pic_mask & (1 << (record->irq_line - 0x10))));

        free_dpmi_memory(record->dpmi_memory_handle);
        record->dpmi_memory_handle = 0;
        if (record->status == -1)
            record->status = 1;
        record->state_saved = 1;
}

int install_interrupt_state(unsigned char *interrupt_record_bytes) {
    int memory_word_address;
    union REGS regs;
    struct SREGS sregs;

    memset(&sregs, 0, 12);
    if (((struct InterruptState *)interrupt_record_bytes)->status != -1) {
        if (((struct InterruptState *)interrupt_record_bytes)->irq_line < 0x10)
            outp(0x21, inp(0x21) | (1 << (((struct InterruptState *)interrupt_record_bytes)->irq_line - 8)));
        else if (((struct InterruptState *)interrupt_record_bytes)->irq_line < 0x18)
            outp(0xa1, inp(0xa1) | (1 << (((struct InterruptState *)interrupt_record_bytes)->irq_line - 0x10)));

        if (((struct InterruptState *)interrupt_record_bytes)->hook_flags & 2) {
            regs.x.eax = 0x205;
            regs.h.bl = ((struct InterruptState *)interrupt_record_bytes)->interrupt_number;
            regs.w.cx = FP_SEG((void (__far *)(void))(void (__near *)(void))((struct InterruptState *)interrupt_record_bytes)->handler_address);
            regs.x.edx = ((struct InterruptState *)interrupt_record_bytes)->handler_address;
            int386x(0x31, &regs, &regs, &sregs);
            ((struct InterruptState *)interrupt_record_bytes)->status = -1;
        }
        if (((struct InterruptState *)interrupt_record_bytes)->hook_flags & 4) {
            regs.x.eax = ((struct InterruptState *)interrupt_record_bytes)->interrupt_number;
            regs.h.ah = 0x25;
            sregs.ds = FP_SEG((void (__far *)(void))(void (__near *)(void))((struct InterruptState *)interrupt_record_bytes)->handler_address);
            regs.x.edx = ((struct InterruptState *)interrupt_record_bytes)->handler_address;
            int386x(0x21, &regs, &regs, &sregs);
            ((struct InterruptState *)interrupt_record_bytes)->status = -1;
        }
        if ((((struct InterruptState *)interrupt_record_bytes)->hook_flags & 1) == 1) {
            ((struct InterruptState *)interrupt_record_bytes)->mapping_length = ((struct InterruptState *)interrupt_record_bytes)->physical_end - ((struct InterruptState *)interrupt_record_bytes)->physical_start;
            if (((struct InterruptState *)interrupt_record_bytes)->allocated_base = allocate_dpmi_memory(((struct InterruptState *)interrupt_record_bytes)->mapping_length + 0x10)) {
                ((struct InterruptState *)interrupt_record_bytes)->dpmi_memory_handle = dpmi_linear_address_value;
                ((struct DpmiMapping *)interrupt_record_bytes)->mapped_address =
                    ((struct DpmiMapping *)interrupt_record_bytes)->allocation_base + 0xd;
                f_13889(((struct InterruptState *)interrupt_record_bytes)->physical_start, ((struct InterruptState *)interrupt_record_bytes)->allocated_base, ((struct InterruptState *)interrupt_record_bytes)->mapping_length);
                memory_word_address = ((struct InterruptState *)interrupt_record_bytes)->allocated_base + 7;
                *(unsigned short *)memory_word_address = ((struct InterruptState *)interrupt_record_bytes)->allocated_base >> 4;
                memory_word_address = ((struct InterruptState *)interrupt_record_bytes)->allocated_base + ((struct InterruptState *)interrupt_record_bytes)->mapping_length - 5;
                *(unsigned short *)memory_word_address = ((struct InterruptState *)interrupt_record_bytes)->descriptor_selector;
                *(unsigned short *)(memory_word_address + 2) = ((struct InterruptState *)interrupt_record_bytes)->descriptor_offset;
                regs.x.eax = 0x201;
                regs.h.bl = ((struct InterruptState *)interrupt_record_bytes)->interrupt_number;
                regs.x.ecx = (((struct InterruptState *)interrupt_record_bytes)->allocated_base >> 4) & 0xffff;
                regs.x.edx = ((struct InterruptState *)interrupt_record_bytes)->allocated_base & 0xf;
                int386(0x31, &regs, &regs);
            }
            ((struct InterruptState *)interrupt_record_bytes)->status = -1;
        } else {
            ((struct InterruptState *)interrupt_record_bytes)->dpmi_memory_handle = 0;
            dpmi_memory_error = 0;
        }

        if (((struct InterruptState *)interrupt_record_bytes)->irq_line < 0x10)
            outp(0x21, inp(0x21) & ~(1 << (((struct InterruptState *)interrupt_record_bytes)->irq_line - 8)));
        else if (((struct InterruptState *)interrupt_record_bytes)->irq_line < 0x18)
            outp(0xa1, inp(0xa1) & ~(1 << (((struct InterruptState *)interrupt_record_bytes)->irq_line - 0x10)));
        return 0;
    }
    return -1;
}
