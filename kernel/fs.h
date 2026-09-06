#ifndef FS_H
#define FS_H
#include "../include/types.h"
#include "ramdisk.h"

#define MAX_INODES     8
#define MAX_FILES      8
#define DIRECT_BLOCKS  8
#define MAX_NAME       28
#define MAX_FILE_SIZE  (DIRECT_BLOCKS * BLOCK_SIZE)   /* 8 * 512 = 4KB */

typedef struct {
    uint32_t used;
    uint32_t size;
    uint32_t direct[DIRECT_BLOCKS];
} inode_t;

typedef struct {
    char     name[MAX_NAME];
    uint32_t inode;
    uint32_t used;
} dirent_t;

void fs_init(void);
int  fs_create(const char *name);
int  fs_open(const char *name);
int  fs_read(int fd, void *buf, uint32_t max_len);
int  fs_write(int fd, const void *data, uint32_t len);
int  fs_unlink(const char *name);
int  fs_list(dirent_t *out, int max);
uint32_t fs_size_of(int inode_index);

#endif
