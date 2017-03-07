//
// Created by cormac on 24/02/17.
//

#ifndef RT8900_SERIAL_CONTROL_SERIAL_H
#define RT8900_SERIAL_CONTROL_SERIAL_H

#include <stdbool.h>
#include <sys/queue.h>

#include "control_packet.h"

struct control_packet_q_node{
    struct control_packet *packet;
    int do_not_free; //should we free the packet once it is sent (default flase)
    TAILQ_ENTRY(control_packet_q_node) nodes; //link to next packet (for que)
};

///create our packet queue struct
typedef TAILQ_HEAD(CONTROL_PACKET_Q_HEAD, control_packet_q_node) CONTROL_PACKET_Q_HEAD;

typedef struct {
    bool verbose;
    bool lazy;
    char *serial_path;
    pthread_barrier_t* initialised;
    struct CONTROL_PACKET_Q_HEAD *queue;
    int serial_fd;
    bool keep_alive;
} SERIAL_CFG;

#endif //RT8900_SERIAL_CONTROLL_SERIAL_H
