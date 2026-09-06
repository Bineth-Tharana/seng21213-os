# SENG21213-OS — Bineth Tharana

> **Course**: SENG 21213 – Computer Architecture & Operating Systems
> **Year**: 2nd Year, Software Engineering
> **Assignment**: Build your own x86 Operating System

**Stages completed: Stage 0, Stage 1, Stage 2, Stage 3**

| Lecture | Milestone | Status | Tag |
|---------|-----------|--------|-----|
| L08 | Stage 0 - Boot + VGA + Shell | Complete | `v0.1-stage0` |
| L09 | Stage 1 - Process Management & Scheduler | Complete | `v0.2-stage1` |
| L10 | Stage 2 - Threads & Synchronisation | Complete | `v0.3-stage2` |
| L11 | Stage 3 - Memory Management | Complete | `v0.4-stage3` |
| L12 | Stage 4 - File System | Not started | - |

---

## Project Structure

seng21213-os/
├── boot/
│ └── boot.asm MBR bootloader: real-mode setup, E820 memory
│ detection, GDT + protected-mode switch
├── kernel/
│ ├── kernel_entry.asm Protected-mode entry point, calls kernel_main()
│ ├── kernel.c Shell loop, command dispatch, all demo processes
│ ├── vga.c / vga.h VGA 80x25 text driver (incl. vga_put_at/vga_puts_at
│ │ for cursor-independent concurrent writes)
│ ├── keyboard.c / .h PS/2 keyboard polling driver
│ ├── process.c / .h pcb_t process table, create_process()
│ ├── scheduler.c / .h Round-robin scheduler, scheduler_tick()
│ ├── idt.c / .h IDT, 8259 PIC remap, PIT (100Hz), irq0_handler()
│ ├── switch.asm switch_context() - PUSHAD/POPAD + RET
│ ├── isr.asm irq0_stub - raw IRQ0 entry point
│ ├── io.h inb/outb port I/O helpers
│ ├── thread.c / .h thread_create()/thread_yield()
│ ├── mutex.c / .h Blocking mutex (test-and-set + block/wake)
│ ├── semaphore.c / .h Counting semaphore
│ └── pmm.c / .h Bitmap physical frame allocator
├── include/
│ └── types.h Primitive types (no libc)
├── linker.ld Linker script (kernel at 0x10000)
├── Makefile Build system
└── README.md You are here


---

## Quick Start (Ubuntu / WSL2)

```bash
sudo apt install nasm gcc gcc-multilib binutils qemu-system-x86 make

# Build (uses -mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only,
# see Debugging Notes below for why these flags are required)
make clean && make

# Run
make run
```

---

## Understanding the Boot Process

Power On
|
v
BIOS (firmware in ROM)
| Loads 512-byte MBR from disk sector 1 into RAM at 0x7C00
v
boot/boot.asm (Real Mode, 16-bit)
| Prints "Loading SENG21213-OS..."
| Reads 64 sectors (kernel) from disk into RAM at 0x10000
| Detects physical memory via BIOS int 0x15, EAX=0xE820
| -> raw entries stored at 0x9000, entry count at 0x8FF0
| Sets up GDT, switches CPU to 32-bit Protected Mode
| Far-jumps to 0x10000
v
kernel/kernel_entry.asm (Protected Mode, 32-bit)
| Calls kernel_main()
v
kernel/kernel.c -> kernel_main()
| vga_init(), kb_init(), pmm_init()
| process_init(), scheduler_init()
| creates shell + demo processes, calls scheduler_add() for each
| idt_init() - IDT, PIC remap, PIT programmed for 100Hz
| scheduler_bootstrap() + switch_context() into the first process
v
Pre-emptive multitasking from here on, driven by the 100Hz timer.


---

## Stage 1: Process Management & Scheduler

The starter kit shipped with no interrupt handling at all (no IDT, no PIC
remap, no timer), so Stage 1 required building that infrastructure from
scratch before the scheduler itself could work.

**API:**
```c
typedef enum { PROC_UNUSED, PROC_READY, PROC_RUNNING, PROC_BLOCKED, PROC_TERMINATED } proc_state_t;

typedef struct {
    uint32_t     pid;
    proc_state_t state;
    uint32_t     esp;
    char         name[16];
} pcb_t;

void   process_init(void);
pcb_t *create_process(void (*entry_fn)(void), const char *name);
pcb_t *process_table_entry(int index);

void   scheduler_init(void);
void   scheduler_add(pcb_t *p);
void   scheduler_tick(void);          /* called from irq0_handler on every tick */
pcb_t *scheduler_bootstrap(void);
pcb_t *scheduler_current(void);       /* added in Stage 2, used by mutex/semaphore */
```

**Shell command:** `ps` -- lists all processes with PID, state, name.

**Demo:** `proc_a`/`proc_b` print to fixed positions on row 24 at different
rates while the shell (also a scheduled process) stays fully interactive.

## Stage 2: Threads, Mutex & Semaphore

**API:**
```c
typedef pcb_t thread_t;
thread_t *thread_create(void (*fn)(void *arg), void *arg);
void      thread_yield(void);

typedef struct { volatile int locked; pcb_t *waiters[8]; int waiter_count; } mutex_t;
void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

typedef struct { volatile int count; pcb_t *waiters[8]; int waiter_count; } semaphore_t;
void sem_init(semaphore_t *s, int initial);
void sem_wait(semaphore_t *s);
void sem_signal(semaphore_t *s);
```

**Shell commands:**
- `race` -- two threads each increment a shared `myglobal` 50000 times,
  first without a mutex, then with one. Result: **50000** (lost updates)
  without the mutex vs **100000** (correct) with it.
- `pc` -- bounded-buffer producer-consumer using 3 semaphores over an
  8-slot buffer; produces and consumes 15 items with zero corruption.

## Stage 3: Physical Memory Manager

**API:**
```c
typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t type;      /* 1 = usable RAM */
    uint32_t acpi_ext;
} e820_entry_t;

void     pmm_init(void);
uint32_t pmm_alloc_frame(void);
void     pmm_free_frame(uint32_t phys_addr);
uint32_t pmm_total_frames(void);
uint32_t pmm_used_frames(void);
uint32_t pmm_free_frames(void);
int      pmm_selftest(void);
```

`boot.asm` calls BIOS `int 0x15, EAX=0xE820` in a loop during real mode
(before the protected-mode switch) and stores the raw 24-byte entries at
physical address `0x9000`, with the entry count as a word at `0x8FF0`.
`pmm_init()` reads that data back in protected mode to build a 1-bit-per-
4KB-frame bitmap, reserving the first 1MB unconditionally (BIOS/IVT area,
boot sector, the E820 buffer itself, and the loaded kernel image).

**Shell commands:**
- `meminfo` -- total / used / free physical memory in MB and frames
- `pmmtest` -- allocates and frees 100 frames in a loop, confirms the
  free-frame count returns to its original value (no leaks)

**Verified result (32MB QEMU instance):** Total 31MB (8160 frames),
Used 1MB (256 frames, the reserved first-1MB region), Free 30MB
(7904 frames). `pmmtest` reports PASS.

---

## Debugging Notes

**Stage 1:**

1. *Triple fault / QEMU reset loop from SSE code generation.* GCC 15.x
   auto-vectorises loops at `-O2` and can emit SSE/XMM instructions by
   default. `switch_context` only saves GPRs via PUSHAD/POPAD, and a
   freestanding kernel has no guaranteed 16-byte stack alignment for SSE
   -- either mismatch causes an instant #GP escalating to a triple fault
   (visible as QEMU silently resetting in a loop). Diagnosed with
   `qemu-system-i386 ... -no-reboot -no-shutdown -d int,cpu_reset -D qemu_debug.log`.
   Fixed by adding `-mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only` to `CFLAGS`.

2. *Null pointer dereference from a missing `scheduler_add()` call.*
   `create_process()` only builds a PCB, it does not queue it. The first
   `kernel_main()` never called `scheduler_add()`, so `scheduler_bootstrap()`
   returned NULL, which was then dereferenced (confirmed via `EIP=00000003`
   in the QEMU debug log). Fixed by wrapping each `create_process()` call
   in `scheduler_add()`.

3. *Demo processes corrupting shell output via a shared cursor.*
   `proc_a`/`proc_b` originally used `vga_set_cursor()`, which moved the
   same global cursor the shell's own output relies on. Fixed with
   `vga_put_at()`/`vga_puts_at()`, which write directly to VGA memory,
   bypassing the shared cursor.

**Stage 2:**

4. *The unsafe race demo initially showed no corruption (false negative).*
   A plain `myglobal_unsafe++` in a tight loop let one thread finish all
   50000 iterations within a single 10ms scheduler tick, so the two
   threads never actually interleaved mid-increment. Fixed by splitting
   the increment into explicit read / large busy-wait (5000 iterations) /
   write steps, making a timer preemption mid-critical-section far more
   likely. Reliably reproduces myglobal_unsafe = 50000 instead of 100000.

**Stage 3:**

5. *512-byte boot sector overflow after adding E820 detection.*
   `nasm` failed with `TIMES value -20 is negative` -- the new real-mode
   E820 probe code pushed boot.asm 20 bytes over the mandatory 512-byte
   MBR limit. Fixed by removing the informational "[BOOT] Memory map
   detected" print message (the E820 detection logic itself was kept
   in full; only the cosmetic status string was cut) to fit back under
   the 512-byte ceiling.

---

## Known Limitations

**Stage 1:** Fixed 4KB per-process stacks, statically reserved. No
priority levels or sleep/wake queue in the scheduler. No general process
termination cleanup path.

**Stage 2:** `thread_create()` supports at most 16 pending (fn, arg)
pairs keyed by `pid % 16`. Mutex/semaphore waiter lists are fixed at 8
entries each. No priority inheritance.

**Stage 3:** Bitmap sized for up to 64MB of RAM (`PMM_MAX_FRAMES`). E820
map capped at 32 entries. No virtual memory / paging yet -- purely
physical frame allocation.

---

## Debugging with GDB

```bash
make run-debug
# In another terminal:
gdb
(gdb) target remote :1234
(gdb) set architecture i386
(gdb) symbol-file build/kernel.elf
(gdb) break kernel_main
(gdb) continue
```

## Debugging triple faults / silent resets

```bash
qemu-system-i386 -drive format=raw,file=seng21213-os.img -m 32M \
  -no-reboot -no-shutdown -d int,cpu_reset -D qemu_debug.log
# then, in another terminal:
tail -60 qemu_debug.log
```

Look for the `v=XX` exception vector right before "Triple fault" --
e.g. `v=0d` is a General Protection Fault, `v=08` is a Double Fault.

---

*Every commercial OS started exactly like this.*
