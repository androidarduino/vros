// kernel.c

// VGA text mode buffer address
volatile char *vga_buffer = (volatile char *)0xB8000;

// Function to print a single character to the VGA buffer
void print_char(char c, int col, int row)
{
    int index = (row * 80 + col) * 2;
    vga_buffer[index] = c;
    vga_buffer[index + 1] = 0x07; // Light grey on black
}

// Function to print a string to the VGA buffer
void print_string(const char *str, int row)
{
    int col = 0;
    while (*str != '\0')
    {
        print_char(*str, col++, row);
        str++;
    }
}

void kernel_main(void)
{
    // Print a welcome message
    print_string("Hello from my microkernel!", 0);

    // Loop indefinitely
    while (1)
    {
        // Do nothing
    }
}
