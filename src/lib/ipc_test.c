#include "task.h"
#include "ipc.h"

static volatile int server_running = 1;
static volatile int client_running = 1;

// Server task - uses NAMED port and runs until stopped
static void ipc_server_task(void)
{
    // Create a named port "echo_service"
    int port_id = ipc_create_named_port("echo_service");

    if (port_id < 0)
    {
        // Failed to create port - run forever doing nothing
        while (1)
            task_yield();
    }

    // Receive messages continuously
    struct ipc_message msg;
    int msg_count = 0;

    while (server_running)
    {
        // Non-blocking receive
        int result = ipc_try_recv(port_id, &msg);

        if (result == 0)
        {
            // Message received
            msg_count++;
        }

        task_yield();
    }

    // Clean up
    ipc_destroy_port(port_id);

    // Exit
    while (1)
        task_yield();
}

// Client task - finds server by name and sends messages periodically
static void ipc_client_task(void)
{
    // Give server time to create port
    for (int i = 0; i < 50; i++)
        task_yield();

    // Find server port by name
    int server_port = ipc_find_port("echo_service");

    if (server_port < 0)
    {
        // Server not found - run forever doing nothing
        while (1)
            task_yield();
    }

    int msg_count = 0;

    // Send messages periodically
    while (client_running)
    {
        char msg[64];
        msg[0] = 'M';
        msg[1] = 's';
        msg[2] = 'g';
        msg[3] = ' ';
        msg[4] = '0' + (msg_count % 10);
        msg[5] = '\0';

        ipc_send(server_port, 1, msg, 6);
        msg_count++;

        // Yield many times between messages
        for (int i = 0; i < 100; i++)
            task_yield();
    }

    // Exit
    while (1)
        task_yield();
}

// Start IPC test
void ipc_test_start(void)
{
    server_running = 1;
    client_running = 1;
    task_create("ipc_server", ipc_server_task);
    task_create("ipc_client", ipc_client_task);
}

// Stop IPC test
void ipc_test_stop(void)
{
    // Signal tasks to stop
    server_running = 0;
    client_running = 0;

    // Give them time to clean up
    for (int i = 0; i < 100; i++)
        task_yield();

    // Find and kill IPC test tasks
    for (int i = 0; i < 32; i++)
    {
        struct task *t = task_find_by_pid(i);
        if (t && ((t->name[0] == 'i' && t->name[1] == 'p' &&
                   t->name[2] == 'c' && t->name[3] == '_')))
        {
            t->state = TASK_ZOMBIE;
        }
    }
}
