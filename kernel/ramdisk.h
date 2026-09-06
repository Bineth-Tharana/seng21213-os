#ifndef RAMDISK_H
#define RAMDISK_H
#include "../include/types.h"

#define RAMDISK_SIZE   (64 * 1024)    /* 64 KB - must fit alongside kernel code/data/other BSS within the 512KB gap between the kernel load address (0x10000) and the boot-set-up stack (0x90000) */
#define BLOCK_SIZE     512
#define BLOCK_COUNT    (RAMDISK_SIZE / BLOCK_SIZE)

void ramdisk_init(void);
void ramdisk_read(uint32_t block, void *buf);
void ramdisk_write(uint32_t block, const void *buf);

#endif
