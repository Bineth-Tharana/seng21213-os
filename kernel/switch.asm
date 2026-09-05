; L09 Sec3 - low level context switch.
; void switch_context(uint32_t *old_esp_store, uint32_t new_esp);
;
; Uses PUSHAD/POPAD + RET (not IRET) - deliberately simpler than an
; interrupt-frame based switch. A brand-new process's fake stack (built
; in process.c) just looks like a stack switch_context itself already
; pushad'd onto, with entry_fn as the return address below it - so the
; very first switch into a new process falls straight into entry_fn.
; A previously-preempted process resumes here too, and its own RET then
; naturally unwinds back up through scheduler_tick -> irq0_handler ->
; irq0_stub's IRETD, which restores THAT process's original interrupt
; frame from when it was preempted.
[bits 32]
section .text
global switch_context

switch_context:
    mov eax, [esp+4]      ; old_esp_store (may be 0/NULL)
    mov edx, [esp+8]      ; new_esp

    pushad
    test eax, eax
    jz .skip_save
    mov [eax], esp
.skip_save:
    mov esp, edx
    popad
    sti                    ; x86 guarantees the next instruction (ret)
                            ; executes before any interrupt is serviced
    ret
