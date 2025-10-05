#ifndef SHELL_H
#define SHELL_H

// Maximum command line length
#define MAX_COMMAND_LENGTH 256

// Function declarations
void shell_init(void);
void shell_run(void);
void shell_handle_input(char c);
void shell_clear_screen(void);

#endif // SHELL_H
