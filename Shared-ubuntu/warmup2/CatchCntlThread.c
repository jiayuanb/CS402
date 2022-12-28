#include<pthread.h>
#include<signal.h>
#include<stdio.h>
#include<stdlib.h>
#include"Packet.h"


void * CatchCntlThread(void *arg){
    CancelThreadParams * params = (CancelThreadParams *)arg;
    sigset_t * set = params->set;
    int s, sig;

    for(;;){
        s = sigwait(set, &sig);
        if(s != 0){
            handle_error_en(s, "sigwait");
        }
        flockfile(stdout);
        fprintf(stdout, "\tSIGINT caught, no new packets or tokens will be allowed\n");
        funlockfile(stdout);
        pthread_mutex_lock(params->params->mutex);
        TIME_TO_STOP = TRUE;
        pthread_cancel(*(params->packetThread));
        pthread_cancel(*(params->tokenThread));
        pthread_cond_broadcast(params->params->cv);

        RemovePacketsFromQueue("Q1", params->params->Q1);
        RemovePacketsFromQueue("Q2", params->params->Q2);
        pthread_mutex_unlock(params->params->mutex);
        pthread_exit(0);
    }
}