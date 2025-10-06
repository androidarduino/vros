#ifndef IPC_H
#define IPC_H

#include <stdint.h>

// IPC message structure
#define IPC_MSG_MAX_SIZE 256

struct ipc_message
{
    uint32_t sender_pid;         // Sender process ID
    uint32_t sender_port;        // Sender port ID (for replies)
    uint32_t type;               // Message type (user-defined)
    uint32_t size;               // Data size in bytes
    char data[IPC_MSG_MAX_SIZE]; // Message data
};

// IPC port structure
#define IPC_MAX_PORTS 32
#define IPC_PORT_QUEUE_SIZE 16
#define IPC_PORT_NAME_MAX 32

struct ipc_port
{
    uint32_t port_id;             // Port ID
    uint32_t owner_pid;           // Owner process ID
    int in_use;                   // Port is allocated
    char name[IPC_PORT_NAME_MAX]; // Port name (optional)

    // Message queue
    struct ipc_message queue[IPC_PORT_QUEUE_SIZE];
    uint32_t queue_head;  // Next message to read
    uint32_t queue_tail;  // Next position to write
    uint32_t queue_count; // Number of messages in queue

    // Statistics
    uint32_t total_sent;     // Total messages sent to this port
    uint32_t total_received; // Total messages received
    uint32_t drops;          // Dropped messages (queue full)

    // Waiting process
    struct task *waiting_task; // Task blocked on recv
};

// IPC system calls
int ipc_create_port(void);                   // Create a new port, returns port_id
int ipc_create_named_port(const char *name); // Create a named port
int ipc_destroy_port(uint32_t port_id);      // Destroy a port
int ipc_send(uint32_t dest_port, uint32_t type, const void *data, uint32_t size);
int ipc_send_from_port(uint32_t src_port, uint32_t dest_port, uint32_t type, const void *data, uint32_t size);
int ipc_recv(uint32_t port_id, struct ipc_message *msg);     // Blocking receive
int ipc_try_recv(uint32_t port_id, struct ipc_message *msg); // Non-blocking receive
int ipc_find_port(const char *name);                         // Find port by name

// Statistics
struct ipc_stats
{
    uint32_t total_ports;    // Total ports created
    uint32_t active_ports;   // Currently active ports
    uint32_t total_messages; // Total messages sent
    uint32_t blocked_tasks;  // Tasks blocked on recv
};

void ipc_get_stats(struct ipc_stats *stats);

// Internal functions
void ipc_init(void);
struct ipc_port *ipc_get_port(uint32_t port_id);

#endif // IPC_H
