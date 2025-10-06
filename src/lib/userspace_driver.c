#include "task.h"
#include "ipc.h"

// Simulated user-space keyboard driver
// In a real microkernel, this would handle actual hardware interrupts via IPC

#define MSG_TYPE_KEY_EVENT 1
#define MSG_TYPE_REGISTER_CLIENT 2
#define MSG_TYPE_UNREGISTER_CLIENT 3

static volatile int driver_running = 1;
static int client_ports[8]; // Up to 8 clients
static int num_clients = 0;

// Keyboard driver task - runs in user space
static void userspace_keyboard_driver(void)
{
    // Create named port for keyboard service
    int driver_port = ipc_create_named_port("kbd_driver");

    if (driver_port < 0)
    {
        // Failed to create port
        while (1)
            task_yield();
    }

    // Initialize client list
    for (int i = 0; i < 8; i++)
        client_ports[i] = -1;
    num_clients = 0;

    struct ipc_message msg;
    int key_counter = 0;

    while (driver_running)
    {
        // Check for client registration messages (non-blocking)
        int result = ipc_try_recv(driver_port, &msg);

        if (result == 0)
        {
            if (msg.type == MSG_TYPE_REGISTER_CLIENT)
            {
                // Register new client
                if (num_clients < 8)
                {
                    // Client port ID is in the message data
                    int client_port = *((int *)msg.data);
                    client_ports[num_clients++] = client_port;
                }
            }
            else if (msg.type == MSG_TYPE_UNREGISTER_CLIENT)
            {
                // Unregister client
                int client_port = *((int *)msg.data);
                for (int i = 0; i < num_clients; i++)
                {
                    if (client_ports[i] == client_port)
                    {
                        // Shift array
                        for (int j = i; j < num_clients - 1; j++)
                            client_ports[j] = client_ports[j + 1];
                        num_clients--;
                        break;
                    }
                }
            }
        }

        // Simulate key events (in real system, this would come from hardware)
        // Send a key event every few iterations
        key_counter++;
        if (key_counter >= 200)
        {
            key_counter = 0;

            // Broadcast key event to all registered clients
            char key_event[8];
            key_event[0] = 'K'; // Simulated key
            key_event[1] = 'E';
            key_event[2] = 'Y';
            key_event[3] = '\0';

            for (int i = 0; i < num_clients; i++)
            {
                if (client_ports[i] >= 0)
                {
                    ipc_send(client_ports[i], MSG_TYPE_KEY_EVENT,
                             key_event, 4);
                }
            }
        }

        task_yield();
    }

    // Clean up
    ipc_destroy_port(driver_port);

    while (1)
        task_yield();
}

// Client application - receives key events from driver
static void keyboard_client_app(void)
{
    // Give driver time to start
    for (int i = 0; i < 100; i++)
        task_yield();

    // Find keyboard driver
    int driver_port = ipc_find_port("kbd_driver");
    if (driver_port < 0)
    {
        // Driver not found
        while (1)
            task_yield();
    }

    // Create our port to receive events
    int my_port = ipc_create_port();
    if (my_port < 0)
    {
        while (1)
            task_yield();
    }

    // Register with driver
    ipc_send(driver_port, MSG_TYPE_REGISTER_CLIENT, &my_port, sizeof(int));

    // Receive key events
    struct ipc_message msg;
    int event_count = 0;

    while (driver_running && event_count < 5)
    {
        int result = ipc_try_recv(my_port, &msg);

        if (result == 0 && msg.type == MSG_TYPE_KEY_EVENT)
        {
            // Received a key event
            event_count++;
        }

        task_yield();
    }

    // Unregister
    ipc_send(driver_port, MSG_TYPE_UNREGISTER_CLIENT, &my_port, sizeof(int));

    // Clean up
    ipc_destroy_port(my_port);

    while (1)
        task_yield();
}

// Start user-space driver test
void userspace_driver_start(void)
{
    driver_running = 1;
    task_create("kbd_driver", userspace_keyboard_driver);
    task_create("kbd_client1", keyboard_client_app);
    task_create("kbd_client2", keyboard_client_app);
}

// Stop user-space driver test
void userspace_driver_stop(void)
{
    driver_running = 0;

    // Give tasks time to clean up
    for (int i = 0; i < 100; i++)
        task_yield();

    // Kill all driver-related tasks
    for (int i = 0; i < 32; i++)
    {
        struct task *t = task_find_by_pid(i);
        if (t && ((t->name[0] == 'k' && t->name[1] == 'b' && t->name[2] == 'd')))
        {
            t->state = TASK_ZOMBIE;
        }
    }
}
