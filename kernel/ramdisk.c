/* L12 Sec1 - a plain byte array in BSS standing in for a block device. */
#include "ramdisk.h"

static uint8_t disk[RAMDISK_SIZE];

static void rd_memset(void *dst, int val, uint32_t n) {
    uint8_t *d = (uint8_t*)dst;
    for (uint32_t i = 0; i < n; i++) d[i] = (uint8_t)val;
}
static void rd_memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

void ramdisk_init(void) {
    rd_memset(disk, 0, sizeof(disk));
}

void ramdisk_read(uint32_t block, void *buf) {
    if (block >= BLOCK_COUNT) return;
    rd_memcpy(buf, &disk[block * BLOCK_SIZE], BLOCK_SIZE);
}

void ramdisk_write(uint32_t block, const void *buf) {
    if (block >= BLOCK_COUNT) return;
    rd_memcpy(&disk[block * BLOCK_SIZE], buf, BLOCK_SIZE);
}
