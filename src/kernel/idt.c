#include "idt.h"

// IDT entries array
static struct idt_entry idt[IDT_ENTRIES];

// IDT descriptor
static struct idt_descriptor idt_desc;

// External assembly function to load IDT
extern void idt_load(uint32_t);

// Set an IDT gate
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags)
{
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

// Initialize IDT
void idt_init(void)
{
    // Set up IDT descriptor
    idt_desc.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idt_desc.base = (uint32_t)&idt;

    // Clear IDT
    for (int i = 0; i < IDT_ENTRIES; i++)
    {
        idt_set_gate(i, 0, 0, 0);
    }

    // Load IDT
    idt_load((uint32_t)&idt_desc);
}
