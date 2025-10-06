#include "ipc.h"
#include "task.h"
#include "kmalloc.h"
#include <stddef.h>

// Global port table
static struct ipc_port ports[IPC_MAX_PORTS];
static uint32_t total_messages_sent = 0;

// Initialize IPC system
void ipc_init(void)
{
    for (int i = 0; i < IPC_MAX_PORTS; i++)
    {
        ports[i].port_id = i;
        ports[i].owner_pid = 0;
        ports[i].in_use = 0;
        ports[i].name[0] = '\0';
        ports[i].queue_head = 0;
        ports[i].queue_tail = 0;
        ports[i].queue_count = 0;
        ports[i].total_sent = 0;
        ports[i].total_received = 0;
        ports[i].drops = 0;
        ports[i].waiting_task = NULL;
    }
    total_messages_sent = 0;
}

// Get port by ID
struct ipc_port *ipc_get_port(uint32_t port_id)
{
    if (port_id >= IPC_MAX_PORTS)
        return NULL;

    if (!ports[port_id].in_use)
        return NULL;

    return &ports[port_id];
}

// Create a new port
int ipc_create_port(void)
{
    struct task *current = task_get_current();

    // Find free port
    for (int i = 0; i < IPC_MAX_PORTS; i++)
    {
        if (!ports[i].in_use)
        {
            ports[i].in_use = 1;
            ports[i].owner_pid = current->pid;
            ports[i].name[0] = '\0';
            ports[i].queue_head = 0;
            ports[i].queue_tail = 0;
            ports[i].queue_count = 0;
            ports[i].total_sent = 0;
            ports[i].total_received = 0;
            ports[i].drops = 0;
            ports[i].waiting_task = NULL;
            return i; // Return port ID
        }
    }

    return -1; // No free ports
}

// Create a named port
int ipc_create_named_port(const char *name)
{
    if (!name)
        return -1;

    struct task *current = task_get_current();

    // Check if name already exists
    for (int i = 0; i < IPC_MAX_PORTS; i++)
    {
        if (ports[i].in_use && ports[i].name[0] != '\0')
        {
            int match = 1;
            for (int j = 0; j < IPC_PORT_NAME_MAX && name[j] && ports[i].name[j]; j++)
            {
                if (name[j] != ports[i].name[j])
                {
                    match = 0;
                    break;
                }
            }
            if (match)
                return -1; // Name already exists
        }
    }

    // Find free port
    for (int i = 0; i < IPC_MAX_PORTS; i++)
    {
        if (!ports[i].in_use)
        {
            ports[i].in_use = 1;
            ports[i].owner_pid = current->pid;

            // Copy name
            int j;
            for (j = 0; j < IPC_PORT_NAME_MAX - 1 && name[j]; j++)
                ports[i].name[j] = name[j];
            ports[i].name[j] = '\0';

            ports[i].queue_head = 0;
            ports[i].queue_tail = 0;
            ports[i].queue_count = 0;
            ports[i].total_sent = 0;
            ports[i].total_received = 0;
            ports[i].drops = 0;
            ports[i].waiting_task = NULL;
            return i; // Return port ID
        }
    }

    return -1; // No free ports
}

// Find port by name
int ipc_find_port(const char *name)
{
    if (!name)
        return -1;

    for (int i = 0; i < IPC_MAX_PORTS; i++)
    {
        if (ports[i].in_use && ports[i].name[0] != '\0')
        {
            int match = 1;
            for (int j = 0; j < IPC_PORT_NAME_MAX && name[j] && ports[i].name[j]; j++)
            {
                if (name[j] != ports[i].name[j])
                {
                    match = 0;
                    break;
                }
            }
            if (match)
                return i;
        }
    }

    return -1; // Not found
}

// Destroy a port
int ipc_destroy_port(uint32_t port_id)
{
    if (port_id >= IPC_MAX_PORTS)
        return -1;

    struct task *current = task_get_current();
    struct ipc_port *port = &ports[port_id];

    // Check ownership
    if (!port->in_use || port->owner_pid != current->pid)
        return -1;

    // Wake up any waiting task
    if (port->waiting_task)
    {
        port->waiting_task->state = TASK_READY;
        port->waiting_task = NULL;
    }

    // Clear port
    port->in_use = 0;
    port->owner_pid = 0;
    port->queue_count = 0;

    return 0;
}

// Send message to port
int ipc_send(uint32_t dest_port, uint32_t type, const void *data, uint32_t size)
{
    if (dest_port >= IPC_MAX_PORTS)
        return -1;

    struct ipc_port *port = &ports[dest_port];
    if (!port->in_use)
        return -1;

    if (size > IPC_MSG_MAX_SIZE)
        return -1;

    // Check if queue is full
    if (port->queue_count >= IPC_PORT_QUEUE_SIZE)
    {
        port->drops++;
        return -1; // Queue full
    }

    struct task *current = task_get_current();

    // Add message to queue
    struct ipc_message *msg = &port->queue[port->queue_tail];
    msg->sender_pid = current->pid;
    msg->type = type;
    msg->size = size;

    // Copy data
    if (data && size > 0)
    {
        for (uint32_t i = 0; i < size && i < IPC_MSG_MAX_SIZE; i++)
        {
            msg->data[i] = ((const char *)data)[i];
        }
    }

    // Update queue
    port->queue_tail = (port->queue_tail + 1) % IPC_PORT_QUEUE_SIZE;
    port->queue_count++;
    port->total_sent++;
    total_messages_sent++;

    // Wake up waiting task if any
    if (port->waiting_task)
    {
        port->waiting_task->state = TASK_READY;
        port->waiting_task = NULL;
    }

    return 0; // Success
}

// Receive message from port (blocking)
int ipc_recv(uint32_t port_id, struct ipc_message *msg)
{
    if (port_id >= IPC_MAX_PORTS)
        return -1;

    struct ipc_port *port = &ports[port_id];
    struct task *current = task_get_current();

    // Check ownership
    if (!port->in_use || port->owner_pid != current->pid)
        return -1;

    // If no messages, block
    if (port->queue_count == 0)
    {
        port->waiting_task = current;
        current->state = TASK_BLOCKED;
        task_yield(); // Give up CPU

        // After waking up, check again
        if (port->queue_count == 0)
            return -1; // Port was destroyed
    }

    // Get message from queue
    struct ipc_message *queued_msg = &port->queue[port->queue_head];

    // Copy to user buffer
    msg->sender_pid = queued_msg->sender_pid;
    msg->type = queued_msg->type;
    msg->size = queued_msg->size;

    for (uint32_t i = 0; i < queued_msg->size && i < IPC_MSG_MAX_SIZE; i++)
    {
        msg->data[i] = queued_msg->data[i];
    }

    // Update queue
    port->queue_head = (port->queue_head + 1) % IPC_PORT_QUEUE_SIZE;
    port->queue_count--;
    port->total_received++;

    return 0; // Success
}

// Non-blocking receive
int ipc_try_recv(uint32_t port_id, struct ipc_message *msg)
{
    if (port_id >= IPC_MAX_PORTS)
        return -1;

    struct ipc_port *port = &ports[port_id];
    struct task *current = task_get_current();

    // Check ownership
    if (!port->in_use || port->owner_pid != current->pid)
        return -1;

    // If no messages, return immediately
    if (port->queue_count == 0)
        return -2; // No messages available

    // Get message from queue
    struct ipc_message *queued_msg = &port->queue[port->queue_head];

    // Copy to user buffer
    msg->sender_pid = queued_msg->sender_pid;
    msg->type = queued_msg->type;
    msg->size = queued_msg->size;

    for (uint32_t i = 0; i < queued_msg->size && i < IPC_MSG_MAX_SIZE; i++)
    {
        msg->data[i] = queued_msg->data[i];
    }

    // Update queue
    port->queue_head = (port->queue_head + 1) % IPC_PORT_QUEUE_SIZE;
    port->queue_count--;
    port->total_received++;

    return 0; // Success
}

// Get IPC statistics
void ipc_get_stats(struct ipc_stats *stats)
{
    if (!stats)
        return;

    stats->total_ports = IPC_MAX_PORTS;
    stats->active_ports = 0;
    stats->total_messages = total_messages_sent;
    stats->blocked_tasks = 0;

    for (int i = 0; i < IPC_MAX_PORTS; i++)
    {
        if (ports[i].in_use)
        {
            stats->active_ports++;
            if (ports[i].waiting_task)
                stats->blocked_tasks++;
        }
    }
}
