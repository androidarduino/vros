#include "keyboard.h"
#include "shell.h"

// Helper functions for port I/O
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// US QWERTY keyboard layout (lowercase)
static const char scancode_to_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '};

// US QWERTY keyboard layout (uppercase/shift)
static const char scancode_to_ascii_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '};

// Keyboard state
static int shift_pressed = 0;
static int caps_lock = 0;

// Simple keyboard input buffer
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static int buffer_read_pos = 0;
static int buffer_write_pos = 0;

// Shell mode flag
static int shell_mode = 0;

// Initialize keyboard
void keyboard_init(void)
{
    buffer_read_pos = 0;
    buffer_write_pos = 0;
    shift_pressed = 0;
    caps_lock = 0;
    shell_mode = 0;
}

// Enable shell mode
void keyboard_enable_shell(void)
{
    shell_mode = 1;
}

// Keyboard interrupt handler
void keyboard_handler(void)
{
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Check if this is a key release (bit 7 set)
    if (scancode & 0x80)
    {
        // Key release
        scancode &= 0x7F;

        if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT)
        {
            shift_pressed = 0;
        }
    }
    else
    {
        // Key press
        if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT)
        {
            shift_pressed = 1;
        }
        else if (scancode == KEY_CAPS_LOCK)
        {
            caps_lock = !caps_lock;
        }
        else if (scancode < sizeof(scancode_to_ascii))
        {
            char c;

            // Determine which character to use
            if (shift_pressed)
            {
                c = scancode_to_ascii_shift[scancode];
            }
            else
            {
                c = scancode_to_ascii[scancode];

                // Apply caps lock to letters
                if (caps_lock && c >= 'a' && c <= 'z')
                {
                    c = c - 'a' + 'A';
                }
            }

            if (c != 0)
            {
                // Pass to shell if enabled
                if (shell_mode)
                {
                    shell_handle_input(c);
                }

                // Add to buffer
                keyboard_buffer[buffer_write_pos] = c;
                buffer_write_pos = (buffer_write_pos + 1) % KEYBOARD_BUFFER_SIZE;
            }
        }
    }
}

// Get a character from keyboard buffer
char keyboard_getchar(void)
{
    if (buffer_read_pos == buffer_write_pos)
    {
        return 0; // Buffer empty
    }

    char c = keyboard_buffer[buffer_read_pos];
    buffer_read_pos = (buffer_read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}
