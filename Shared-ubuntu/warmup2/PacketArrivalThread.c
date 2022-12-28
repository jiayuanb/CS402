#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "Packet.h"
#include "PacketArrivalThread.h"
#include "Packet.h"


unsigned int createPacketByLine(char * reader, packet * p, struct timeval * last){
    char * cur = reader;
    // check all char
    while(*cur != '\0' && *cur == '\n'){
        if(*cur == ' ' || *cur == '\t' || (*cur <= '9' && *cur >= '0')){
            cur++;
        }
        else{
            flockfile(stdout);
            fprintf(stdout, "invalid char %d is in line: %s", (int)*cur, reader);
            funlockfile(stdout);
            exit(0);
        }
    }
    cur = reader;
    while(*cur != '\0' && *cur != '\n' && *cur != ' ' && *cur != '\t'){
        cur++;
    }
    *cur = '\0';
    cur++;
    unsigned int sleep = strtoul(reader, NULL, 10) * 1000;
    // struct timeval curTime;
    // struct timeval diff;
    // gettimeofday(&curTime, NULL);
    // timevalBetween(last, &curTime, &diff);
    // usleep(sleep - diff.tv_sec * 1000000 - diff.tv_usec);
    
    // skip all space and tab
    while(*cur == ' ' || *cur == '\t'){
        cur++;
    }
    reader = cur;
    if(*cur == '\0' || *cur == '\n'){
        exit(-1);
    }
    while(*cur != '\0' && *cur != '\n' && *cur != ' ' && *cur != '\t'){
        cur++;
    }
    *cur = '\0';
    cur++;
    p->required_tokens = atoi(reader);
    
    // skip all space and tab
    while(*cur == ' ' || *cur == '\t'){
        cur++;
    }
    reader = cur;
    if(*cur == '\0' || *cur == '\n'){
        exit(-1);
    }
    while(*cur != '\0' && *cur != '\n' && *cur != ' ' && *cur != '\t'){
        cur++;
    }
    *cur = '\0';
    cur++;
    unsigned int service = strtoul(reader, NULL, 10);
    p->service_time.tv_sec = service / 1000;
    p->service_time.tv_nsec = service % 1000 * 1000000;

    return sleep;
}

void * PacketArrivalThread(void * arg){
    int s = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    if(s != 0){
        handle_error_en(s, "pthread_setcancelstate");
    }
    PacketThreadParams * packetParams = (PacketThreadParams *) arg;
    ThreadParams * params = packetParams->params;
    PacketThreadResult * result = packetParams->result;
    if(params->tsfile == NULL){
        // result->total = *(params->num);
        unsigned int sleep = 1 / *(params->lambda) / 0.000001;
        struct timeval last;
        struct timeval cur;
        struct timeval diff;
        last.tv_usec = params->start->tv_usec;
        last.tv_sec = params->start->tv_sec;
        for(int i=0; i<*(params->num); i++){
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
            usleep(sleep);
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
            
            pthread_mutex_lock(params->mutex);
            result->total++;
            // print packet arrival time
            flockfile(stdout);
            gettimeofday(&cur, 0);
            timevalBetween(params->start, &cur, &diff);
            fprintf(stdout, "%05ld%03ld.%03ldms: ", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
            timevalBetween(&last, &cur, &diff);
            last.tv_usec = cur.tv_usec;
            last.tv_sec = cur.tv_sec;
            

            if(*(params->P) <= *(params->B)){
                fprintf(stdout, "p%d arrives, needs %d tokens, inter-arrival time = %ld.%03ldms\n", i+1, *(params->P), diff.tv_sec * 1000 + diff.tv_usec / 1000, diff.tv_usec % 1000);
            } else {
                fprintf(stdout, "p%d arrives, needs %d tokens, inter-arrival time = %ld.%03ldms, dropped\n", i+1, *(params->P), diff.tv_sec * 1000 + diff.tv_usec / 1000, diff.tv_usec % 1000);
                (params->num_drop)++;
            }
            funlockfile(stdout);
            // unsigned long avg_time = divideTimeval(&diff, *(params->num));
            // addTimeval(result->inter_arrival_time_sum, avg_time);
            result->avg_inter_arrival_sec += (diff.tv_sec + diff.tv_usec * 0.000001) / *(params->num);

            // create packet
            packet *p = (packet *) malloc(sizeof(packet));
            memset(p, 0, sizeof(packet));
            p->index = i + 1;
            p->required_tokens = *(params->P);
            p->arrive_time.tv_sec = cur.tv_sec;
            p->arrive_time.tv_usec = cur.tv_usec;
            unsigned int integer_part = (unsigned int)(1 / *(params->mu));
            p->service_time.tv_sec = integer_part;
            p->service_time.tv_nsec = (long)((1 / *(params->mu) - integer_part) * 1000000000);

            // insert into Q1
            if(*(params->P) <= *(params->B)){
                gettimeofday(&cur, 0);
                timevalBetween(params->start, &cur, &diff);
                p->last_act.tv_sec = cur.tv_sec;
                p->last_act.tv_usec = cur.tv_usec;
                My402ListAppend(params->Q1, p);
                flockfile(stdout);
                fprintf(stdout, "%05ld%03ld.%03ldms: ", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
                fprintf(stdout, "p%d enters Q1\n", i+1);
                funlockfile(stdout);
                pthread_cond_broadcast(params->cv);
            }
            pthread_mutex_unlock(params->mutex);
        }
    }
    else{
        
        struct timeval last;
        struct timeval cur;
        struct timeval diff;
        last.tv_usec = params->start->tv_usec;
        last.tv_sec = params->start->tv_sec;

        // read tsfile first line
        char *reader = NULL;
        size_t len = 0;
        ssize_t nread;
        if((nread = getline(&reader, &len, params->tsfile)) != -1){
            if(nread > 1024){
                flockfile(stdout);
                fprintf(stdout, "first line of tsfile is longer than expected\n");
                funlockfile(stdout);
                exit(-1);
            }
            *(params->num) = atoi(reader);
        }
        for(int i=0; i<*(params->num) && (nread = getline(&reader, &len, params->tsfile)) != -1; i++){
            if(nread > 1024){
                flockfile(stdout);
                fprintf(stdout, "line %d is longer than expected: %s\n", i+1, reader);
                funlockfile(stdout);
                exit(-1);
            }

            
            packet *p = (packet *) malloc(sizeof(packet));
            memset(p, 0, sizeof(packet));
            p->index = i + 1;

            // sleep to wait to create package
            unsigned int sleep = createPacketByLine(reader, p, &last);

            gettimeofday(&cur, NULL);
            timevalBetween(&last, &cur, &diff);
            sleep -= diff.tv_sec * 1000000 + diff.tv_usec;
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
            usleep(sleep);
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
            pthread_mutex_lock(params->mutex);

            result->total++;

            // package arrives
            gettimeofday(&cur, 0);
            timevalBetween(params->start, &cur, &diff);
            flockfile(stdout);
            fprintf(stdout, "%05ld%03ld.%03ldms: ", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
            timevalBetween(&last, &cur, &diff);
            last.tv_usec = cur.tv_usec;
            last.tv_sec = cur.tv_sec;
            if(p->required_tokens <= *(params->B)){
                fprintf(stdout, "p%d arrives, needs %d tokens, inter-arrival time = %ld.%03ldms\n", i+1, *(params->P), diff.tv_sec * 1000 + diff.tv_usec / 1000, diff.tv_usec % 1000);
            }
            else{
                fprintf(stdout, "p%d arrives, needs %d tokens, inter-arrival time = %ld.%03ldms, dropped\n", i+1, *(params->P), diff.tv_sec * 1000 + diff.tv_usec / 1000, diff.tv_usec % 1000);
                (params->num_drop)++;
            }
            // unsigned long avg_time = divideTimeval(&diff, *(params->num));
            // addTimeval(result->inter_arrival_time_sum, avg_time);
            result->avg_inter_arrival_sec += (diff.tv_sec + diff.tv_usec * 0.000001) / *(params->num);
            funlockfile(stdout);
            p->arrive_time.tv_sec = cur.tv_sec;
            p->arrive_time.tv_usec = cur.tv_usec;

            // insert into Q1
            if(p->required_tokens <= *(params->B)){
                gettimeofday(&cur, 0);
                timevalBetween(params->start, &cur, &diff);
                p->last_act.tv_sec = cur.tv_sec;
                p->last_act.tv_usec = cur.tv_usec;
                My402ListAppend(params->Q1, p);
                flockfile(stdout);
                fprintf(stdout, "%05ld%03ld.%03ldms: ", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
                fprintf(stdout, "p%d enters Q1\n", i+1);
                funlockfile(stdout);
                pthread_cond_broadcast(params->cv);
            }
            pthread_mutex_unlock(params->mutex);
        }
    }
    return 0;
}