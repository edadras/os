#include "idt.h"
#include "ports.h"
#include "string.h"
#include "vga.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr ip;
static irq_handler_t irq_handlers[16];

extern void idt_load(uint32_t ip_addr);

#define ISR(n) extern void isr##n(void);
#define IRQ(n) extern void irq##n(void);
ISR(0) ISR(1) ISR(2) ISR(3) ISR(4) ISR(5) ISR(6) ISR(7)
ISR(8) ISR(9) ISR(10) ISR(11) ISR(12) ISR(13) ISR(14) ISR(15)
ISR(16) ISR(17) ISR(18) ISR(19) ISR(20) ISR(21) ISR(22) ISR(23)
ISR(24) ISR(25) ISR(26) ISR(27) ISR(28) ISR(29) ISR(30) ISR(31)
IRQ(0) IRQ(1) IRQ(2) IRQ(3) IRQ(4) IRQ(5) IRQ(6) IRQ(7)
IRQ(8) IRQ(9) IRQ(10) IRQ(11) IRQ(12) IRQ(13) IRQ(14) IRQ(15)
#undef ISR
#undef IRQ

static void idt_set_gate(int n, uint32_t base)
{
    idt[n].base_low = base & 0xFFFF;
    idt[n].base_high = (base >> 16) & 0xFFFF;
    idt[n].sel = 0x08;
    idt[n].zero = 0;
    idt[n].flags = 0x8E;    /* present, ring 0, 32-bit interrupt gate */
}

static void pic_remap(void)
{
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();    /* master offset 32 */
    outb(0xA1, 0x28); io_wait();    /* slave offset 40 */
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();
    outb(0x21, 0x00);               /* unmask all */
    outb(0xA1, 0x00);
}

void idt_init(void)
{
    ip.limit = sizeof(idt) - 1;
    ip.base = (uint32_t)&idt;
    memset(idt, 0, sizeof(idt));

    pic_remap();

#define SET_ISR(n) idt_set_gate(n, (uint32_t)isr##n)
#define SET_IRQ(n) idt_set_gate(32 + n, (uint32_t)irq##n)
    SET_ISR(0); SET_ISR(1); SET_ISR(2); SET_ISR(3);
    SET_ISR(4); SET_ISR(5); SET_ISR(6); SET_ISR(7);
    SET_ISR(8); SET_ISR(9); SET_ISR(10); SET_ISR(11);
    SET_ISR(12); SET_ISR(13); SET_ISR(14); SET_ISR(15);
    SET_ISR(16); SET_ISR(17); SET_ISR(18); SET_ISR(19);
    SET_ISR(20); SET_ISR(21); SET_ISR(22); SET_ISR(23);
    SET_ISR(24); SET_ISR(25); SET_ISR(26); SET_ISR(27);
    SET_ISR(28); SET_ISR(29); SET_ISR(30); SET_ISR(31);
    SET_IRQ(0); SET_IRQ(1); SET_IRQ(2); SET_IRQ(3);
    SET_IRQ(4); SET_IRQ(5); SET_IRQ(6); SET_IRQ(7);
    SET_IRQ(8); SET_IRQ(9); SET_IRQ(10); SET_IRQ(11);
    SET_IRQ(12); SET_IRQ(13); SET_IRQ(14); SET_IRQ(15);
#undef SET_ISR
#undef SET_IRQ

    idt_load((uint32_t)&ip);
}

void irq_register_handler(int irq, irq_handler_t handler)
{
    irq_handlers[irq] = handler;
}

static const char *exception_names[32] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 FPU Error", "Alignment Check", "Machine Check", "SIMD FP Exception",
    "Virtualization", "Control Protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
};

void isr_handler(registers_t *regs)
{
    terminal_setcolor(VGA_WHITE, VGA_RED);
    terminal_write("\nKERNEL PANIC: ");
    terminal_write(exception_names[regs->int_no]);
    terminal_write(" (int ");
    terminal_write_dec(regs->int_no);
    terminal_write(", err ");
    terminal_write_hex(regs->err_code);
    terminal_write(") at EIP=");
    terminal_write_hex(regs->eip);
    terminal_write("\nSystem halted.\n");
    for (;;)
        __asm__ volatile ("cli; hlt");
}

void irq_handler(registers_t *regs)
{
    int irq = (int)regs->int_no - 32;

    if (irq >= 8)
        outb(0xA0, 0x20);   /* EOI to slave PIC */
    outb(0x20, 0x20);       /* EOI to master PIC */

    if (irq >= 0 && irq < 16 && irq_handlers[irq])
        irq_handlers[irq](regs);
}
