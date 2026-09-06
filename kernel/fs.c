/* L12 Sec2-3 - inode-based flat file system on the ramdisk.
 * Layout: block 0 = directory, block 1 = inode table (fits easily in
 * one 512-byte block: 16 inodes * (4+4+8*4) = 16*40 = 640 bytes...
 * that's actually just over 512, so the inode table spans blocks 1-2.
 * Data blocks start at block 3. */
#include "fs.h"

#define BLK_DIR          0
#define BLK_INODE_TABLE  1   /* spans blocks 1-2 */
#define BLK_DATA_START   3

static dirent_t dir[MAX_FILES];
static inode_t  inodes[MAX_INODES];
static uint8_t  inode_used[MAX_INODES];
static uint8_t  block_used[BLOCK_COUNT];

static void fs_memset(void *dst, int val, uint32_t n) {
    uint8_t *d = (uint8_t*)dst;
    for (uint32_t i = 0; i < n; i++) d[i] = (uint8_t)val;
}
static void fs_memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}
static int fs_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}
static void fs_strncpy(char *dst, const char *src, int n) {
    int i = 0;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/* Persist the directory and inode table to the ramdisk. Called after
 * every mutating operation so `ls`/`cat` reflect the true on-disk state
 * even though we also keep an in-memory mirror for speed. */
static void persist(void) {
    uint8_t buf[BLOCK_SIZE];

    fs_memset(buf, 0, BLOCK_SIZE);
    fs_memcpy(buf, dir, sizeof(dir));
    ramdisk_write(BLK_DIR, buf);

    /* inode table across 2 blocks */
    fs_memset(buf, 0, BLOCK_SIZE);
    uint32_t first_chunk = sizeof(inodes) > BLOCK_SIZE ? BLOCK_SIZE : sizeof(inodes);
    fs_memcpy(buf, inodes, first_chunk);
    ramdisk_write(BLK_INODE_TABLE, buf);

    if (sizeof(inodes) > BLOCK_SIZE) {
        fs_memset(buf, 0, BLOCK_SIZE);
        fs_memcpy(buf, (uint8_t*)inodes + BLOCK_SIZE, sizeof(inodes) - BLOCK_SIZE);
        ramdisk_write(BLK_INODE_TABLE + 1, buf);
    }
}

void fs_init(void) {
    ramdisk_init();
    fs_memset(dir, 0, sizeof(dir));
    for (int i = 0; i < MAX_FILES; i++) dir[i].inode = 0xFFFFFFFF;

    fs_memset(inodes, 0, sizeof(inodes));
    fs_memset(inode_used, 0, sizeof(inode_used));
    fs_memset(block_used, 0, sizeof(block_used));

    for (uint32_t b = 0; b < BLK_DATA_START; b++) block_used[b] = 1;

    persist();
}

static int alloc_inode(void) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (!inode_used[i]) { inode_used[i] = 1; return i; }
    }
    return -1;
}

static uint32_t alloc_block(void) {
    for (uint32_t b = 0; b < BLOCK_COUNT; b++) {
        if (!block_used[b]) { block_used[b] = 1; return b; }
    }
    return 0; /* 0 is reserved (BLK_DIR), so 0 safely means "none free" */
}

static void free_block(uint32_t b) {
    if (b > 0 && b < BLOCK_COUNT) block_used[b] = 0;
}

static int find_dirent(const char *name) {
    for (int i = 0; i < MAX_FILES; i++)
        if (dir[i].used && fs_strcmp(dir[i].name, name) == 0) return i;
    return -1;
}

int fs_create(const char *name) {
    if (find_dirent(name) != -1) return -1;

    int de = -1;
    for (int i = 0; i < MAX_FILES; i++) if (!dir[i].used) { de = i; break; }
    if (de == -1) return -1;

    int ino = alloc_inode();
    if (ino == -1) return -1;

    fs_strncpy(dir[de].name, name, MAX_NAME);
    dir[de].inode = (uint32_t)ino;
    dir[de].used = 1;

    inodes[ino].used = 1;
    inodes[ino].size = 0;
    fs_memset(inodes[ino].direct, 0, sizeof(inodes[ino].direct));

    persist();
    return ino;
}

int fs_open(const char *name) {
    int de = find_dirent(name);
    if (de == -1) return -1;
    return (int)dir[de].inode;
}

int fs_read(int fd, void *buf, uint32_t max_len) {
    if (fd < 0 || fd >= MAX_INODES || !inodes[fd].used) return -1;

    uint32_t to_read = inodes[fd].size < max_len ? inodes[fd].size : max_len;
    uint8_t block_buf[BLOCK_SIZE];
    uint32_t copied = 0;

    for (int i = 0; i < DIRECT_BLOCKS && copied < to_read; i++) {
        if (inodes[fd].direct[i] == 0) break;
        ramdisk_read(inodes[fd].direct[i], block_buf);
        uint32_t chunk = to_read - copied;
        if (chunk > BLOCK_SIZE) chunk = BLOCK_SIZE;
        fs_memcpy((uint8_t*)buf + copied, block_buf, chunk);
        copied += chunk;
    }
    return (int)copied;
}

int fs_write(int fd, const void *data, uint32_t len) {
    if (fd < 0 || fd >= MAX_INODES || !inodes[fd].used) return -1;
    inode_t *ino = &inodes[fd];
    if (ino->size + len > MAX_FILE_SIZE) return -1;

    uint32_t written = 0;
    const uint8_t *src = (const uint8_t*)data;

    while (written < len) {
        uint32_t block_index = ino->size / BLOCK_SIZE;
        uint32_t offset      = ino->size % BLOCK_SIZE;
        if (block_index >= DIRECT_BLOCKS) break;

        if (ino->direct[block_index] == 0) {
            uint32_t b = alloc_block();
            if (b == 0) break;
            ino->direct[block_index] = b;
            uint8_t zero[BLOCK_SIZE];
            fs_memset(zero, 0, BLOCK_SIZE);
            ramdisk_write(b, zero);
        }

        uint8_t block_buf[BLOCK_SIZE];
        ramdisk_read(ino->direct[block_index], block_buf);

        uint32_t space = BLOCK_SIZE - offset;
        uint32_t chunk = (len - written) < space ? (len - written) : space;
        fs_memcpy(block_buf + offset, src + written, chunk);
        ramdisk_write(ino->direct[block_index], block_buf);

        written += chunk;
        ino->size += chunk;
    }

    persist();
    return (int)written;
}

int fs_unlink(const char *name) {
    int de = find_dirent(name);
    if (de == -1) return -1;
    int ino = (int)dir[de].inode;

    for (int i = 0; i < DIRECT_BLOCKS; i++)
        if (inodes[ino].direct[i]) free_block(inodes[ino].direct[i]);

    inodes[ino].used = 0;
    inode_used[ino] = 0;

    dir[de].used = 0;
    dir[de].inode = 0xFFFFFFFF;

    persist();
    return 0;
}

int fs_list(dirent_t *out, int max) {
    int n = 0;
    for (int i = 0; i < MAX_FILES && n < max; i++)
        if (dir[i].used) out[n++] = dir[i];
    return n;
}

uint32_t fs_size_of(int inode_index) {
    if (inode_index < 0 || inode_index >= MAX_INODES) return 0;
    return inodes[inode_index].size;
}
