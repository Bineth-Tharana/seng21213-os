/* L09 Sec3 - IDT, PIC remap, and PIT programmed for a 100Hz IRQ0 tick */
#include "idt.h"
#include "io.h"
#include "scheduler.h"
#include "../include/types.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

extern void irq0_stub(void);

static void idt_set_gate(int n, uint32_t handler, uint16_t sel, uint8_t flags) {
    idt[n].base_low  = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].sel       = sel;
    idt[n].always0   = 0;
    idt[n].flags     = flags;
}

static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20); /* master offset -> 0x20 */
    outb(0xA1, 0x28); /* slave offset  -> 0x28 */
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    outb(0x21, 0xFE); /* unmask only IRQ0 (timer) on master */
    outb(0xA1, 0xFF); /* mask everything on slave */
}

static void pit_init(uint32_t freq_hz) {
    uint32_t divisor = 1193182 / freq_hz;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

/* Called from isr.asm's irq0_stub after it saves registers. */
void irq0_handler(void) {
    outb(0x20, 0x20); /* EOI must be sent before scheduler_tick, which
                          may not return here for a long time */
    scheduler_tick();
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < 256; i++) idt_set_gate(i, 0, 0, 0);

    pic_remap();

    /* selector 0x08 = CODE_SEG from boot.asm's GDT
     * flags 0x8E    = Present | Ring0 | 32-bit Interrupt Gate */
    idt_set_gate(0x20, (uint32_t)(void*)irq0_stub, 0x08, 0x8E);

    __asm__ volatile ("lidt %0" : : "m"(idtp));

    pit_init(100); /* 100Hz = 10ms scheduler tick */
}
