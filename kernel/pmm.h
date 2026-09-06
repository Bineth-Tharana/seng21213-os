#ifndef PMM_H
#define PMM_H
#include "../include/types.h"

#define PMM_FRAME_SIZE 4096
#define PMM_MAX_FRAMES 16384        /* supports up to 64 MB of RAM */
#define PMM_MAX_E820_ENTRIES 32

/* Matches the raw 24-byte record BIOS int 0x15/E820 writes when asked
 * for ecx=24 bytes (base:8, length:8, type:4, ACPI 3.x ext attr:4). */
typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t type;      /* 1 = usable RAM */
    uint32_t acpi_ext;  /* may be garbage on older BIOS - unused */
} e820_entry_t;

void     pmm_init(void);
uint32_t pmm_alloc_frame(void);
void     pmm_free_frame(uint32_t phys_addr);
uint32_t pmm_total_frames(void);
uint32_t pmm_used_frames(void);
uint32_t pmm_free_frames(void);
int      pmm_selftest(void); /* returns 1 on pass, 0 on fail */

#endif
