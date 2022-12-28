#include <sys/time.h>
#include <unistd.h>
#include "Packet.h"

int timevalBetween(struct timeval *start, struct timeval *end, struct timeval *diff){
    if(end->tv_sec < start->tv_sec){
        return FALSE;
    }
    if(end->tv_sec == start->tv_sec && end->tv_usec < start->tv_usec){
        return FALSE;
    }


    if(end->tv_usec < start->tv_usec){
        diff->tv_usec = 1000000 - start->tv_usec + end->tv_usec;
        diff->tv_sec = end->tv_sec - start->tv_sec - 1;
    }
    else{
        diff->tv_usec = end->tv_usec - start->tv_usec;
        diff->tv_sec = end->tv_sec - start->tv_sec;
    }
    return TRUE;
}


unsigned long divideTimeval(struct timeval * time, int num){
    if(num == 0){
        return 0;
    }
    long int sec = time->tv_sec;
    long int usec = time->tv_usec;
    usec = (sec % num * 1000000 + usec) / num;
    sec = sec / num;
    unsigned long result = sec * 1000000 + usec;
    return result;
}

void addTimeval(struct timeval * start, unsigned long time){
    long int sec = start->tv_sec;
    long int usec = start->tv_usec;
    long int sum = sec * 1000000 + usec + time;
    start->tv_sec = sum / 1000000;
    start->tv_usec = sum % 1000000;
}

double divideTimevalByTimeval(struct timeval * factor, struct timeval * denominator){
    unsigned int f = factor->tv_sec * 1000000 + factor->tv_usec;
    unsigned int d = denominator->tv_sec * 1000000 + denominator->tv_usec;
    return (double)f / d;
}

void addTwoTimeval(struct timeval * t1, struct timeval * t2, struct timeval * res){
    unsigned long usec_sum = t1->tv_usec + t2->tv_usec;
    res->tv_usec = usec_sum % 1000000;
    res->tv_sec = t1->tv_sec + t2->tv_sec + usec_sum / 1000000;
}


double timevalSquaredDividedByK(struct timeval * val, int k){
    double result = val->tv_sec + val->tv_usec * 0.000001;
    result = result * result / k;
    return result;
}


int handle_error_en(int s, char * arg){
    printf("Cannot run %s, return error no %d\n", arg, s);
    exit(-1);
}

void RemovePacketsFromQueue(char * name, My402List * queue){
    while(!My402ListEmpty(queue)){
        My402ListElem * elem = My402ListFirst(queue);
        packet * p = (packet *)(My402ListFirst(queue)->obj);
        My402ListUnlink(queue, My402ListFirst(queue));
        printf("\tp%d removed from %s\n", p->index, name);
        free(elem);
        free(p);
    }
}
