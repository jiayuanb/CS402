#ifndef _PACKET_H_
#define _PACKET_H_

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "my402list.h"

typedef struct packet{
    int index;
    int required_tokens;
    struct timespec service_time;
    struct timeval last_act;
    struct timeval arrive_time;
} packet;

typedef struct ThreadParams{
    pthread_mutex_t * mutex;
    struct timeval * start;
    int num_drop;
    
    FILE * tsfile; // tsfile address
    double * lambda; // packet arrival rate
    double * r; // token arrival rate
    double * mu; // server packet rate
    int * num; // total number of packets to arrive
    int * B; // bucket depth
    int * P; // tokens to transmit

    My402List * Q1; // queue for arrived packets
    My402List * Q2; // queue for packet service

    int * served_packets;
    pthread_cond_t *cv;
}ThreadParams;

typedef struct ServerThreadResult{
    struct timeval * total_stay_Q2;
    struct timeval * total_service_time;
    struct timeval * total_system_spent;
    double avg_of_timevalSquared;
    double avg_packet_service_time;
    int served_packets;
    double avg_packet_spent_in_system;
    double avg_squared_packet_spent_in_system;
} ServerThreadResult;

typedef struct ServerThreadParams{
    int index;
    ThreadParams * params;
    ServerThreadResult * result;
} ServerThreadParams;

typedef struct PacketThreadResult{
    // struct timeval * inter_arrival_time_sum;
    int total;
    double avg_inter_arrival_sec;
} PacketThreadResult;

typedef struct PacketThreadParams{
    PacketThreadResult * result;
    ThreadParams * params;
} PacketThreadParams;

typedef struct TokenThreadResult{
    struct timeval * total_stay_Q1;
    int drop_tokens;
    int total_tokens;
} TokenThreadResult;

typedef struct TokenThreadParams{
    TokenThreadResult * result;
    ThreadParams * params;
} TokenThreadParams;

typedef struct CancelThreadParams{
    ThreadParams * params;
    pthread_t * tokenThread;
    pthread_t * packetThread;
    sigset_t * set;
} CancelThreadParams;

extern int timevalBetween(struct timeval *start, struct timeval *end, struct timeval *diff);

extern unsigned long divideTimeval(struct timeval * time, int num);

extern void addTimeval(struct timeval * start, unsigned long time);

extern double divideTimevalByTimeval(struct timeval * factor, struct timeval * denominator);

extern void addTwoTimeval(struct timeval * t1, struct timeval * t2, struct timeval * res);

extern int handle_error_en(int s, char * arg);

extern double timevalSquaredDividedByK(struct timeval * val, int k);

extern int TIME_TO_STOP;

extern void RemovePacketsFromQueue(char * name, My402List * queue);

#endif