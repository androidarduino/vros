#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Keyboard data port
#define KEYBOARD_DATA_PORT 0x60

// Keyboard status port
#define KEYBOARD_STATUS_PORT 0x64

// Special keys
#define KEY_BACKSPACE 0x0E
#define KEY_ENTER 0x1C
#define KEY_LSHIFT 0x2A
#define KEY_RSHIFT 0x36
#define KEY_LCTRL 0x1D
#define KEY_LALT 0x38
#define KEY_CAPS_LOCK 0x3A

// Function declarations
void keyboard_init(void);
void keyboard_handler(void);
int keyboard_buffer_empty(void);
char keyboard_getchar(void);
void keyboard_enable_shell(void);

#endif // KEYBOARD_H
