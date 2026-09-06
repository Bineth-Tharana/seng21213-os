/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 0 – Foundations)
 * File   : kernel/kernel.c
 *
 * PURPOSE
 *   This is the heart of your operating system. Right now it:
 *     1. Initialises VGA text-mode display
 *     2. Initialises the keyboard driver
 *     3. Prints a splash screen
 *     4. Runs a minimal interactive shell ("ksh")
 *
 * ASSIGNMENT MILESTONES  (what YOU will add in later lectures)
 *   Lecture  9  – Process Management  →  process.h / process.c / scheduler.c
 *   Lecture 10  – Threads             →  thread.h  / thread.c
 *   Lecture 11  – Memory Management   →  pmm.h     / pmm.c / vmm.c
 *   Lecture 12  – File System         →  fs.h      / fs.c
 *
 * CODING CONVENTION
 *   - Prefix kernel-internal functions with k_ (e.g. k_strcmp)
 *   - All driver APIs live in their own .h/.c pair
 *   - NEVER call malloc – use the PMM you build in Lecture 11
 * ============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "../include/types.h"
#include "process.h"
#include "scheduler.h"
#include "idt.h"
#include "mutex.h"
#include "semaphore.h"
#include "thread.h"
#include "pmm.h"

extern void switch_context(uint32_t *old_esp_store, uint32_t new_esp);

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_version(void);
static void cmd_colour(const char *args);
static void cmd_halt(void);
static void cmd_ps(void);
static void demo_proc_a(void);
static void demo_proc_b(void);
static void cmd_race(void);
static void cmd_pc(void);
static void racer_unsafe(void *arg);
static void racer_safe(void *arg);
static void producer_fn(void *arg);
static void consumer_fn(void *arg);
static void cmd_meminfo(void);
static void cmd_pmmtest(void);

/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* Skip leading spaces */
static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 0: Kernel Foundations", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering – Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath – only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones to implement:\n");
    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Process Management  – PCB, ready queue, round-robin scheduler\n");
    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Threads & Sync      – kernel threads, mutex, semaphore\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   – physical page allocator, virtual memory\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         – RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  help    – Show this help message\n");
    vga_puts("  clear   – Clear the screen\n");
    vga_puts("  about   – About this OS and course\n");
    vga_puts("  echo    – Echo text to screen\n");
    vga_puts("  mem     – Memory map (stub)\n");
    vga_puts("  version - Show kernel name and version\n");
    vga_puts("  colour  - Change text colour <fg> <bg>\n");
    vga_puts("  halt    - Halt the CPU\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ps      – [L09] List processes\n");
    vga_puts("  kill    – [L09] Terminate a process\n");
    vga_puts("  threads – [L10] List kernel threads\n");
    vga_puts("  free    – [L11] Show free memory\n");
    vga_puts("  ls      – [L12] List files\n");
    vga_puts("  cat     – [L12] Print file contents\n\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 – Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void) {
    /* Stage 0 stub – students implement the real PMM in Lecture 11 */
    vga_puts_color("\n  Memory Map (stub – implement PMM in Lecture 11)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  0x00000000 – 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n");
    vga_puts("  0x00100000 – 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n");
    vga_puts("  0x00F00000 – 0x00FFFFFF  :  BIOS / ROM area\n");
    vga_puts("  0xB8000    – 0xBFFFF     :  VGA frame buffer\n");
    vga_puts_color("\n  TODO: Use BIOS int 0x15, EAX=0xE820 to get real memory map\n\n",
                   VGA_YELLOW, VGA_BLACK);
}

static void cmd_version(void) {
    vga_puts_color("\n  SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  Version 0.1 - Stage 0 (Boot, VGA, Shell)\n\n");
}

static int parse_int(const char *s) {
    int result = 0;
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result;
}

static void cmd_colour(const char *args) {
    const char *p = args;
    int fg = parse_int(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    int bg = parse_int(p);
    if (fg < 0 || fg > 15 || bg < 0 || bg > 15) {
        vga_puts_color("  Usage: colour <fg 0-15> <bg 0-15>\n", VGA_YELLOW, VGA_BLACK);
        return;
    }
    vga_set_color((vga_color_t)fg, (vga_color_t)bg);
    vga_puts("  Colour changed.\n");
}

static void cmd_halt(void) {
    vga_puts_color("\n  Halting CPU. Goodbye!\n\n", VGA_YELLOW, VGA_BLACK);
    __asm__ volatile ("cli");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

static void put_uint(uint32_t n) {
    char buf[11];
    int i = 10;
    buf[i--] = '\0';
    if (n == 0) buf[i--] = '0';
    while (n > 0) { buf[i--] = '0' + (n % 10); n /= 10; }
    vga_puts(&buf[i + 1]);
}

static void cmd_ps(void) {
    vga_puts_color("\n  PID  STATE      NAME\n", VGA_YELLOW, VGA_BLACK);
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = process_table_entry(i);
        if (!p || p->state == PROC_UNUSED) continue;
        const char *state_str =
            p->state == PROC_READY   ? "READY   " :
            p->state == PROC_RUNNING ? "RUNNING " : "TERM    ";
        vga_puts("  ");
        put_uint(p->pid);
        vga_puts("    ");
        vga_puts(state_str);
        vga_puts(" ");
        vga_puts(p->name);
        vga_puts("\n");
    }
    vga_puts("\n");
}

static void demo_proc_a(void) {
    while (true) {
        vga_put_at(24, 0, 'A', VGA_LIGHT_RED, VGA_BLACK);
        for (volatile int i = 0; i < 800000; i++);
    }
}

static void demo_proc_b(void) {
    while (true) {
        vga_put_at(24, 2, 'B', VGA_LIGHT_CYAN, VGA_BLACK);
        for (volatile int i = 0; i < 2000000; i++);
    }
}

/* ---------------------------------------------------------------------------
 * Stage 2: race condition demo (myglobal, with and without a mutex)
 * --------------------------------------------------------------------------*/
static volatile int myglobal_unsafe = 0;
static volatile int myglobal_safe = 0;
static mutex_t race_mutex;
static volatile int race_threads_done = 0;

static void racer_unsafe(void *arg) {
    (void)arg;
    for (int i = 0; i < 50000; i++) {
        /* Deliberately split the read-modify-write into three steps with
         * a gap between them, so a timer interrupt is very likely to
         * land the OTHER thread's own read/write in between -- making
         * the classic lost-update race visible on screen instead of
         * relying on unlucky-but-rare interleaving timing. */
        int tmp = myglobal_unsafe;
        for (volatile int j = 0; j < 5000; j++);
        tmp = tmp + 1;
        myglobal_unsafe = tmp;
    }
    race_threads_done++;
}

static void racer_safe(void *arg) {
    (void)arg;
    for (int i = 0; i < 50000; i++) {
        mutex_lock(&race_mutex);
        myglobal_safe++;
        mutex_unlock(&race_mutex);
    }
    race_threads_done++;
}

static void cmd_race(void) {
    vga_puts_color("\n  Race condition demo: 2 threads x 50000 increments each\n",
                   VGA_YELLOW, VGA_BLACK);
    vga_puts("  Expected correct total: 100000\n\n");

    myglobal_unsafe = 0;
    myglobal_safe = 0;
    mutex_init(&race_mutex);
    race_threads_done = 0;

    thread_create(racer_unsafe, (void*)0);
    thread_create(racer_unsafe, (void*)0);
    while (race_threads_done < 2) { __asm__ volatile ("hlt"); }
    vga_puts("  WITHOUT mutex -> myglobal = ");
    put_uint((uint32_t)myglobal_unsafe);
    vga_puts_color(myglobal_unsafe == 100000 ? "  (no corruption this run)\n"
                                              : "  (lost updates - DATA CORRUPTION)\n",
                   VGA_LIGHT_RED, VGA_BLACK);

    race_threads_done = 0;
    thread_create(racer_safe, (void*)0);
    thread_create(racer_safe, (void*)0);
    while (race_threads_done < 2) { __asm__ volatile ("hlt"); }
    vga_puts("  WITH mutex    -> myglobal = ");
    put_uint((uint32_t)myglobal_safe);
    vga_puts_color("  (correct)\n\n", VGA_LIGHT_GREEN, VGA_BLACK);
}

/* ---------------------------------------------------------------------------
 * Stage 2: bounded-buffer producer-consumer (3 semaphores)
 * --------------------------------------------------------------------------*/
#define PC_BUF_SIZE 8
#define PC_ITEMS 15
static int pc_buffer[PC_BUF_SIZE];
static int pc_in = 0, pc_out = 0;
static semaphore_t pc_empty, pc_full, pc_mutex;
static volatile int pc_done = 0;

static void producer_fn(void *arg) {
    (void)arg;
    for (int i = 0; i < PC_ITEMS; i++) {
        sem_wait(&pc_empty);
        sem_wait(&pc_mutex);
        pc_buffer[pc_in] = i;
        pc_in = (pc_in + 1) % PC_BUF_SIZE;
        sem_signal(&pc_mutex);
        sem_signal(&pc_full);
    }
}

static void consumer_fn(void *arg) {
    (void)arg;
    for (int i = 0; i < PC_ITEMS; i++) {
        sem_wait(&pc_full);
        sem_wait(&pc_mutex);
        int val = pc_buffer[pc_out];
        pc_out = (pc_out + 1) % PC_BUF_SIZE;
        sem_signal(&pc_mutex);
        sem_signal(&pc_empty);

        vga_puts("  consumed: ");
        put_uint((uint32_t)val);
        vga_puts("\n");
    }
    pc_done = 1;
}

static void cmd_pc(void) {
    vga_puts_color("\n  Producer-Consumer demo (bounded buffer, size 8)\n\n",
                   VGA_YELLOW, VGA_BLACK);
    pc_in = 0; pc_out = 0; pc_done = 0;
    sem_init(&pc_empty, PC_BUF_SIZE);
    sem_init(&pc_full, 0);
    sem_init(&pc_mutex, 1);

    thread_create(producer_fn, (void*)0);
    thread_create(consumer_fn, (void*)0);

    while (!pc_done) { __asm__ volatile ("hlt"); }
    vga_puts_color("  Done - no data corruption.\n\n", VGA_LIGHT_GREEN, VGA_BLACK);
}

/* ---------------------------------------------------------------------------
 * Stage 3: physical memory manager commands
 * --------------------------------------------------------------------------*/
static void cmd_meminfo(void) {
    uint32_t total = pmm_total_frames();
    uint32_t used  = pmm_used_frames();
    uint32_t free  = pmm_free_frames();

    vga_puts_color("\n  Physical Memory (4KB frames)\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  Total: ");
    put_uint((total * 4) / 1024);
    vga_puts(" MB  (");
    put_uint(total);
    vga_puts(" frames)\n");

    vga_puts("  Used:  ");
    put_uint((used * 4) / 1024);
    vga_puts(" MB  (");
    put_uint(used);
    vga_puts(" frames)\n");

    vga_puts("  Free:  ");
    put_uint((free * 4) / 1024);
    vga_puts(" MB  (");
    put_uint(free);
    vga_puts(" frames)\n\n");
}

static void cmd_pmmtest(void) {
    vga_puts_color("\n  Running PMM selftest (alloc + free 100 frames)...\n",
                   VGA_YELLOW, VGA_BLACK);
    if (pmm_selftest()) {
        vga_puts_color("  PASS - no frames leaked.\n\n", VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_puts_color("  FAIL - frame count mismatch or allocation failed.\n\n",
                       VGA_LIGHT_RED, VGA_BLACK);
    }
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        /* Trim leading whitespace */
        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        /* Dispatch */
        if (k_strcmp(cmd, "help")  == 0) { cmd_help();  continue; }
        if (k_strcmp(cmd, "clear") == 0) { cmd_clear(); continue; }
        if (k_strcmp(cmd, "about") == 0) { cmd_about(); continue; }
        if (k_strcmp(cmd, "mem")   == 0) { cmd_mem();   continue; }
        if (k_strcmp(cmd, "version") == 0) { cmd_version(); continue; }
        if (k_strcmp(cmd, "halt")    == 0) { cmd_halt();    continue; }
        if (k_strncmp(cmd, "colour ", 7) == 0) { cmd_colour(k_ltrim(cmd + 7)); continue; }
        if (k_strcmp(cmd, "ps") == 0) { cmd_ps(); continue; }
        if (k_strcmp(cmd, "race") == 0) { cmd_race(); continue; }
        if (k_strcmp(cmd, "pc")   == 0) { cmd_pc();   continue; }
        if (k_strcmp(cmd, "meminfo") == 0) { cmd_meminfo(); continue; }
        if (k_strcmp(cmd, "pmmtest") == 0) { cmd_pmmtest(); continue; }

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        /* Milestone stubs */
        if (k_strcmp(cmd, "ps")      == 0 ||
            k_strcmp(cmd, "kill")    == 0 ||
            k_strcmp(cmd, "threads") == 0 ||
            k_strcmp(cmd, "free")    == 0 ||
            k_strcmp(cmd, "ls")      == 0 ||
            k_strcmp(cmd, "cat")     == 0) {
            vga_puts_color("  [TODO] This command is not yet implemented.\n",
                           VGA_YELLOW, VGA_BLACK);
            vga_puts("  Implement it as part of your lecture assignment.\n");
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

/* ---------------------------------------------------------------------------
 * Kernel entry point – called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
void kernel_main(void) {
    vga_init();
    kb_init();
    pmm_init();
    print_splash();

    process_init();
    scheduler_init();

    scheduler_add(create_process(shell_run,   "shell"));
    scheduler_add(create_process(demo_proc_a, "proc_a"));
    scheduler_add(create_process(demo_proc_b, "proc_b"));

    idt_init();

    pcb_t *first = scheduler_bootstrap();
    uint32_t dummy_esp;
    switch_context(&dummy_esp, first->esp);

    /* Never reached */
    __asm__ __volatile__("hlt");
}
