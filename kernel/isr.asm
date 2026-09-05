; L09 Sec3 - IRQ0 (timer) entry stub. Single flat GDT/no per-process
; address spaces yet, so segment registers never need saving here.
[bits 32]
section .text
global irq0_stub
extern irq0_handler

irq0_stub:
    pushad
    call irq0_handler
    popad
    iretd
