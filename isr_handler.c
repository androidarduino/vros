#include "isr.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "task.h"

// Exception messages
const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"};

// External print_string function
extern void print_string(const char *str, int row);

// Install ISRs
void isr_install(void)
{
    // Set ISR gates (0x8E = present, ring 0, 32-bit interrupt gate)
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);
}

// ISR handler
void isr_handler(struct registers regs)
{
    print_string("Received interrupt: ", 4);

    if (regs.int_no < 32)
    {
        print_string(exception_messages[regs.int_no], 5);
        print_string("System Halted!", 6);

        // Halt the system
        for (;;)
        {
            __asm__ volatile("hlt");
        }
    }
}

// Timer tick counter
volatile uint32_t timer_ticks = 0;

// External function to print a character
extern void print_char(char c, int col, int row);

// Helper function to convert number to string
static void int_to_str(uint32_t num, char *str)
{
    int i = 0;
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    int temp = num;
    int digits = 0;
    while (temp > 0)
    {
        digits++;
        temp /= 10;
    }

    i = digits - 1;
    while (num > 0)
    {
        str[i--] = '0' + (num % 10);
        num /= 10;
    }
    str[digits] = '\0';
}

// IRQ handler
void irq_handler(struct registers regs)
{
    // IRQ 0: Timer interrupt
    if (regs.int_no == 32)
    {
        timer_ticks++;

        // Display timer ticks every 18.2 ticks (approximately 1 second)
        if (timer_ticks % 18 == 0)
        {
            char tick_str[20];
            int_to_str(timer_ticks / 18, tick_str);

            // Print timer in top-right corner (row 0)
            // Format: "Time: XXs"
            const char *prefix = "Time:";
            int col = 80 - 12; // Reserve space for "Time: XXXs"

            // Clear the area first
            for (int i = col; i < 80; i++)
            {
                print_char(' ', i, 0);
            }

            // Reset column
            col = 80 - 12;

            // Print "Time:"
            for (int i = 0; prefix[i] != '\0'; i++)
            {
                print_char(prefix[i], col++, 0);
            }

            // Print space
            print_char(' ', col++, 0);

            // Print number
            for (int i = 0; tick_str[i] != '\0'; i++)
            {
                print_char(tick_str[i], col++, 0);
            }

            // Print "s"
            print_char('s', col++, 0);
        }

        // Call task scheduler
        task_schedule();
    }
    // IRQ 1: Keyboard interrupt
    else if (regs.int_no == 33)
    {
        keyboard_handler();
    }

    // Send EOI to PIC
    pic_send_eoi(regs.int_no - 32);
}
