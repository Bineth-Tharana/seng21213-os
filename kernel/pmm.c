/* L11 Sec5 - bitmap physical frame allocator, built from the E820 memory
 * map boot.asm stores at fixed real-mode addresses before switching to
 * protected mode:
 *   0x8FF0 (word)  - number of E820 entries
 *   0x9000 (array) - raw 24-byte E820 entries, back to back */
#include "pmm.h"

#define E820_COUNT_ADDR 0x8FF0
#define E820_MAP_ADDR   0x9000

static uint8_t  bitmap[PMM_MAX_FRAMES / 8];
static uint32_t total_frames = 0;
static uint32_t used_frames  = 0;

static inline void bitmap_set(uint32_t frame)   { bitmap[frame / 8] |=  (uint8_t)(1 << (frame % 8)); }
static inline void bitmap_clear(uint32_t frame) { bitmap[frame / 8] &= (uint8_t)~(1 << (frame % 8)); }
static inline int  bitmap_test(uint32_t frame)  { return bitmap[frame / 8] & (1 << (frame % 8)); }

void pmm_init(void) {
    for (uint32_t i = 0; i < sizeof(bitmap); i++) bitmap[i] = 0xFF; /* default: all reserved */
    total_frames = 0;

    uint16_t count = *(volatile uint16_t *)E820_COUNT_ADDR;
    e820_entry_t *entries = (e820_entry_t *)E820_MAP_ADDR;

    if (count > PMM_MAX_E820_ENTRIES) count = PMM_MAX_E820_ENTRIES;

    for (uint16_t i = 0; i < count; i++) {
        if (entries[i].type != 1) continue; /* only "usable" regions */

        uint64_t start = entries[i].base;
        uint64_t end   = entries[i].base + entries[i].length;

        start = (start + PMM_FRAME_SIZE - 1) & ~((uint64_t)PMM_FRAME_SIZE - 1);
        end   = end & ~((uint64_t)PMM_FRAME_SIZE - 1);

        for (uint64_t addr = start; addr < end; addr += PMM_FRAME_SIZE) {
            uint32_t frame = (uint32_t)(addr / PMM_FRAME_SIZE);
            if (frame >= PMM_MAX_FRAMES) break;
            bitmap_clear(frame);
            if (frame + 1 > total_frames) total_frames = frame + 1;
        }
    }

    /* Reserve the first 1MB unconditionally: real-mode IVT/BIOS data
     * area, the boot sector (0x7C00), our own E820 buffer (0x9000), and
     * the kernel image itself (loaded at 0x10000) all live here. */
    uint32_t reserved_frames = 0x100000 / PMM_FRAME_SIZE;
    for (uint32_t f = 0; f < reserved_frames && f < PMM_MAX_FRAMES; f++) bitmap_set(f);

    used_frames = 0;
    for (uint32_t f = 0; f < total_frames; f++) if (bitmap_test(f)) used_frames++;
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t f = 0; f < total_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
            return f * PMM_FRAME_SIZE;
        }
    }
    return 0; /* out of memory */
}

void pmm_free_frame(uint32_t phys_addr) {
    uint32_t frame = phys_addr / PMM_FRAME_SIZE;
    if (frame >= total_frames) return;
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        used_frames--;
    }
}

uint32_t pmm_total_frames(void) { return total_frames; }
uint32_t pmm_used_frames(void)  { return used_frames; }
uint32_t pmm_free_frames(void)  { return total_frames - used_frames; }

int pmm_selftest(void) {
    uint32_t before = pmm_free_frames();
    uint32_t addrs[100];
    int ok = 1;

    for (int i = 0; i < 100; i++) {
        addrs[i] = pmm_alloc_frame();
        if (addrs[i] == 0) { ok = 0; break; }
    }
    for (int i = 0; i < 100; i++) {
        if (addrs[i] != 0) pmm_free_frame(addrs[i]);
    }

    uint32_t after = pmm_free_frames();
    return ok && (before == after);
}
