#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include "ServerThread.h"
#include "Packet.h"


void * ServerThread(void * arg){
    ServerThreadParams * serverParams = (ServerThreadParams *) arg;
    ThreadParams * params = serverParams->params;
    ServerThreadResult * result = serverParams->result;

    struct timeval cur;
    struct timeval diff;

    for(;;){

        pthread_mutex_lock(params->mutex);
        while(My402ListEmpty(params->Q2) == TRUE && *(params->served_packets) < *(params->num) - params->num_drop && !TIME_TO_STOP){
            pthread_cond_wait(params->cv, params->mutex);
        }
        if(*(params->served_packets) >= *(params->num) - params->num_drop){
            pthread_mutex_unlock(params->mutex);
            pthread_exit(0);
        }
        if(My402ListEmpty(params->Q2) == TRUE && TIME_TO_STOP){
            pthread_mutex_unlock(params->mutex);
            pthread_exit(0);
        }

        // retrieve Q2 first packet
        My402ListElem * elem = My402ListFirst(params->Q2);
        packet *p = (packet *)(elem->obj);
        // first packet leave Q2
        My402ListUnlink(params->Q2, elem);
        gettimeofday(&cur, 0);
        flockfile(stdout);
        timevalBetween(params->start, &cur, &diff);
        fprintf(stdout, "%05ld%03ld.%03ldms: ", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
        timevalBetween(&(p->last_act), &cur, &diff);
        p->last_act.tv_usec = cur.tv_usec;
        p->last_act.tv_sec = cur.tv_sec;
        fprintf(stdout, "p%d leaves Q2, time in Q2 = %ld.%03ldms\n", p->index, diff.tv_sec * 1000 + diff.tv_usec / 1000, diff.tv_usec % 1000);
        funlockfile(stdout);
        addTimeval(result->total_stay_Q2, diff.tv_sec * 1000000 + diff.tv_usec);

        *(params->served_packets) = *(params->served_packets) + 1;
        pthread_mutex_unlock(params->mutex);

        // serve the retrieved packet
        result->served_packets++;
        gettimeofday(&cur, 0);
        flockfile(stdout);
        timevalBetween(params->start, &cur, &diff);
        struct timeval last;
        last.tv_sec = cur.tv_sec;
        last.tv_usec = cur.tv_usec;
        fprintf(stdout, "%05ld%03ld.%03ldms: ", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
        fprintf(stdout, "p%d begins service at S%d, requesting %ld.%03ldms of service\n", (p->index), serverParams->index, p->service_time.tv_sec*1000 + p->service_time.tv_nsec/1000000, p->service_time.tv_nsec%1000000 / 1000);
        funlockfile(stdout);

        usleep(p->service_time.tv_sec * 1000000 + p->service_time.tv_nsec / 1000);
        
        gettimeofday(&cur, 0);
        flockfile(stdout);
        timevalBetween(params->start, &cur, &diff);
        fprintf(stdout, "%05ld%03ld.%03ldms: ", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
        fprintf(stdout, "p%d departs from S%d, ", p->index, serverParams->index);
        timevalBetween(&last, &cur, &diff);
        fprintf(stdout, "service time = %ld.%03ldms, ", diff.tv_sec*1000 + diff.tv_usec / 1000, diff.tv_usec % 1000);

        result->avg_packet_service_time += (diff.tv_sec + diff.tv_usec * 0.000001) / *(params->num);
        addTimeval(result->total_service_time, diff.tv_sec * 1000000 + diff.tv_usec);

        timevalBetween(&(p->arrive_time), &cur, &diff);
        fprintf(stdout, "total time in system = %ld.%03ldms\n", diff.tv_sec*1000 + diff.tv_usec / 1000, diff.tv_usec % 1000);
        funlockfile(stdout);

        addTimeval(result->total_system_spent, diff.tv_sec * 1000000 + diff.tv_usec);

        result->avg_of_timevalSquared += timevalSquaredDividedByK(&diff, *(params->num));
        result->avg_packet_spent_in_system += (diff.tv_sec + diff.tv_usec * 0.000001) / *(params->num);
        result->avg_squared_packet_spent_in_system += (diff.tv_sec + diff.tv_usec * 0.000001) * (diff.tv_sec + diff.tv_usec * 0.000001) / *(params->num);

        free(p);
    }
    return 0;
}