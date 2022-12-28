#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include "Packet.h"
#include "TokenDepositThread.h"

void * TokenDepositThread(void * arg){
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    TokenThreadParams * tokenParams = (TokenThreadParams *) arg;
    ThreadParams * params = tokenParams->params;
    TokenThreadResult * result = tokenParams->result;

    unsigned int sleep = 1 / *(params->r) / 0.000001;
    int count = 0;
    int index = 0;
    int finished_packets = 0;
    struct timeval cur;
    struct timeval diff;
    for(;;){

        if(finished_packets >= *(params->num) - params->num_drop){
            pthread_cond_broadcast(params->cv);
            pthread_exit(0);
        }
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
        usleep(sleep);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
        pthread_mutex_lock(params->mutex);
        count++;
        index++;
        flockfile(stdout);
        gettimeofday(&cur, 0);
        timevalBetween(params->start, &cur, &diff);
        fprintf(stdout, "%05ld%03ld.%03ldms: ", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
        
        result->total_tokens++;
        if(count <= *(params->B)){
            fprintf(stdout, "token t%d arrives, token bucket now has %d token", index, count);
            if(count > 1){
                fprintf(stdout, "s");
            }
            fprintf(stdout, "\n");
        } else{
            result->drop_tokens++;
            fprintf(stdout, "token t%d arrives, dropped\n", index);
        }
        funlockfile(stdout);


        // check first packet
        if(My402ListEmpty(params->Q1) == FALSE){
            My402ListElem * elem = My402ListFirst(params->Q1);
            packet *p = (packet *)(elem->obj);
            if(p->required_tokens <= count){
                // remove first packet
                finished_packets++;
                gettimeofday(&cur, 0);
                timevalBetween(params->start, &cur, &diff);
                flockfile(stdout);
                fprintf(stdout, "%05ld%03ld.%03ldms: ", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
                My402ListUnlink(params->Q1, elem);
                timevalBetween(&(p->last_act), &cur, &diff);
                p->last_act.tv_usec = cur.tv_usec;
                p->last_act.tv_sec = cur.tv_sec;
                count -= p->required_tokens;
                fprintf(stdout, "p%d leaves Q1, time in Q1 = %ld.%03ldms, token bucket now has %d token", p->index, diff.tv_sec * 1000 + diff.tv_usec / 1000, diff.tv_usec % 1000, count);
                if(count > 1){
                    fprintf(stdout, "s");
                }
                fprintf(stdout, "\n");
                funlockfile(stdout);
                addTimeval(result->total_stay_Q1, diff.tv_sec * 1000000 + diff.tv_usec);

                // move to Q2
                My402ListAppend(params->Q2, p);
                gettimeofday(&(p->last_act), 0);
                pthread_cond_broadcast(params->cv);
                flockfile(stdout);
                gettimeofday(&cur, 0);
                timevalBetween(params->start, &cur, &diff);
                p->last_act.tv_usec = cur.tv_usec;
                p->last_act.tv_sec = cur.tv_sec;
                fprintf(stdout, "%05ld%03ld.%03ldms: ", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
                fprintf(stdout, "p%d enters Q2\n", p->index);
                funlockfile(stdout);
            }
        }
        pthread_mutex_unlock(params->mutex);
    }

    return 0;
}