#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

typedef void (*irq_handler_t)(registers_t *);

void idt_init(void);
void irq_register_handler(int irq, irq_handler_t handler);

#endif
