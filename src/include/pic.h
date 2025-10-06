#ifndef PIC_H
#define PIC_H

#include <stdint.h>

// PIC ports
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

// PIC commands
#define PIC_EOI 0x20

// ICW1
#define ICW1_ICW4 0x01
#define ICW1_INIT 0x10

// ICW4
#define ICW4_8086 0x01

// Function declarations
void pic_init(void);
void pic_send_eoi(uint8_t irq);
void irq_install(void);

#endif // PIC_H
