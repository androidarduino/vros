#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// IDT entry structure
struct idt_entry
{
    uint16_t offset_low;  // Lower 16 bits of handler address
    uint16_t selector;    // Kernel segment selector
    uint8_t zero;         // Always 0
    uint8_t type_attr;    // Type and attributes
    uint16_t offset_high; // Upper 16 bits of handler address
} __attribute__((packed));

// IDT descriptor
struct idt_descriptor
{
    uint16_t limit; // Size of IDT - 1
    uint32_t base;  // Base address of IDT
} __attribute__((packed));

// Number of IDT entries (256 for x86)
#define IDT_ENTRIES 256

// Function declarations
void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags);

#endif // IDT_H
