#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include "my402list.h"
#include "PacketArrivalThread.h"
#include "TokenDepositThread.h"
#include "ServerThread.h"
#include "CatchCntlThread.h"
#include "Packet.h"

extern int errno ;

int TIME_TO_STOP = FALSE;

int setOption(char * name, char * value, double * lambda, double * mu, double * r, int *B, int *P, int *num){
    
    if(strcmp(name, "lambda") == 0){ // packet arrival rate
        *lambda = atof(value);
    }
    else if(strcmp(name, "mu") == 0){ // server packer 
        *mu = atof(value);
    }
    else if(strcmp(name, "r") == 0){ // inter-token-arrival time between consecutive tokens is 1/r seconds
        *r = atof(value);
    }
    else if(strcmp(name, "B") == 0){ // token bucket depth
        *B = atoi(value);
    }
    else if(strcmp(name, "P") == 0){ // P tokens to transmit
        *P = atoi(value);
    }
    else if(strcmp(name, "n") == 0){ // the total number of packets to arrive
        *num = atoi(value);
    }
    else{
        return 0;
    }
    return 1;
}

int checkNumber(char * reader){
    while(*reader != '\n' && *reader != '\0'){
        if(*reader <= '9' && *reader >= '0'){
            reader++;
        }
        else if(*reader == '.'){
            reader++;
        }
        else{
            return FALSE;
        }
    }
    return TRUE;
}

int readTFile(char * name, char * value, double * lambda, double * mu, double * r, int *B, int *P, int *num){
    return 1;
}

void print_std_time_spent_in_system(unsigned long avg, double avg_timevalSquared, int num_packets){
    double sqaured_avg = (avg * 0.000001) * (avg * 0.000001);
    // printf("avg_timevalSquared = %lf, squared_avg = %lf\n", avg_timevalSquared, sqaured_avg);
    double result = avg_timevalSquared - sqaured_avg;
    printf("\tstandard deviation for time spent in system = %.6g\n", result);
}

void statistics(PacketThreadResult * packetThreadResult, int num_drop, TokenThreadResult * tokenThreadResult, struct timeval * total_emulation, ServerThreadResult * serverThreadResult1, ServerThreadResult * serverThreadresult2, int expected_num){

    printf("\nStatistics:\n\n");
    printf("\taverage packet inter-arrival time = %.6gs\n", packetThreadResult->avg_inter_arrival_sec * expected_num / packetThreadResult->total);
    
    struct timeval sum;
    // addTwoTimeval(serverThreadResult1->total_service_time, serverThreadresult2->total_service_time, &sum);
    // unsigned long avg_service = divideTimeval(&sum, packetThreadResult->total);
    double avg_service = (serverThreadResult1->avg_packet_service_time + serverThreadresult2->avg_packet_service_time) * expected_num / (serverThreadResult1->served_packets + serverThreadresult2->served_packets);
    printf("\taverage packet service time = %.6gs\n", avg_service);

    printf("\n\taverage number of packets in Q1 = %.6g\n", divideTimevalByTimeval(tokenThreadResult->total_stay_Q1, total_emulation));
    
    addTwoTimeval(serverThreadResult1->total_stay_Q2, serverThreadresult2->total_stay_Q2, &sum);
    printf("\taverage number of packets in Q2 = %.6g\n", divideTimevalByTimeval(&sum, total_emulation));
    
    printf("\taverage number of packets at S1 = %.6g\n", divideTimevalByTimeval(serverThreadResult1->total_service_time, total_emulation));
    printf("\taverage number of packets at S2 = %.6g\n", divideTimevalByTimeval(serverThreadresult2->total_service_time, total_emulation));

    // addTwoTimeval(serverThreadResult1->total_system_spent, serverThreadresult2->total_system_spent, &sum);
    // unsigned long time = divideTimeval(&sum, packetThreadResult->total);
    double avg_packet_spent_in_system = (serverThreadResult1->avg_packet_spent_in_system + serverThreadresult2->avg_packet_spent_in_system) * expected_num / (serverThreadResult1->served_packets + serverThreadresult2->served_packets);
    printf("\n\taverage time a packet spent in system = %.6gs\n", avg_packet_spent_in_system);

    double avg_squared_packet_spent_in_system = (serverThreadResult1->avg_squared_packet_spent_in_system + serverThreadresult2->avg_squared_packet_spent_in_system) * expected_num / (serverThreadResult1->served_packets + serverThreadresult2->served_packets);
    
    // printf("%lf, %lf, %d, %d\n", serverThreadResult1->avg_squared_packet_spent_in_system, serverThreadresult2->avg_squared_packet_spent_in_system, serverThreadResult1->served_packets, serverThreadresult2->served_packets);
    // printf("%d\n", expected_num);
    
    double std = avg_squared_packet_spent_in_system - avg_packet_spent_in_system * avg_packet_spent_in_system;
    printf("\tstandard deviation for time spent in system = %.6g\n", sqrt(std));
    // print_std_time_spent_in_system(time, serverThreadResult1->avg_of_timevalSquared + serverThreadresult2->avg_of_timevalSquared, packetThreadResult->total);

    printf("\n\ttoken drop probability = %.6g\n", (double) tokenThreadResult->drop_tokens / tokenThreadResult->total_tokens);
    printf("\tpacket drop probability = %.6g\n", (double)num_drop / packetThreadResult->total);
}


int main(int argc, char const *argv[])
{
    double lambda = 1;
    double mu = 0.35;
    double r = 1.5;
    int B = 10;
    int P = 3;
    int num = 20;
    char * tFile = NULL;
    if(argc > 1){
        int i=1;
        while(i < argc){
            char * name = (char *)argv[i];
            // check if it is a valid commandline option
            if(strcmp(name, "-lambda") == 0 || strcmp(name, "-mu") == 0 || strcmp(name, "-r") == 0 || strcmp(name, "-B") == 0 || strcmp(name, "-P") == 0 || strcmp(name, "-n") == 0){
                if(i+1 >= argc){
                    fprintf(stdout, "\t(malformed command, value for \"%s\" is not given)\n", name);
                    exit(-1);
                }
                if(checkNumber((char *)argv[i+1]) == FALSE){
                    fprintf(stdout, "\t(malformed command, value for \"%s\" is not given)\n", name);
                    exit(-1);
                }

                char * value = (char *)argv[i+1];
                if(strcmp(name, "-lambda") == 0){ // packet arrival rate
                    lambda = atof(value);
                }
                else if(strcmp(name, "-mu") == 0){ // server packer 
                    mu = atof(value);
                }
                else if(strcmp(name, "-r") == 0){ // inter-token-arrival time between consecutive tokens is 1/r seconds
                    r = atof(value);
                }
                else if(strcmp(name, "-B") == 0){ // token bucket depth
                    B = atoi(value);
                }
                else if(strcmp(name, "-P") == 0){ // P tokens to transmit
                    P = atoi(value);
                }
                else if(strcmp(name, "-n") == 0){ // the total number of packets to arrive
                    num = atoi(value);
                }

            }
            else if(strcmp(name, "-t") == 0){
                if(i+1 >= argc){
                    fprintf(stdout, "\t(malformed command, value for \"%s\" is not given)\n", name);
                    exit(-1);
                }
                tFile = (char *)argv[i+1];

                FILE * filereader = fopen(tFile, "r");
                if(filereader == NULL){
                    int errnum = errno;
                    if(errnum == EACCES){
                        fprintf(stdout, "\t(input file %s cannot be opened - access denies)\n", tFile);
                    }
                    else if(errnum == ENOENT){
                        fprintf(stdout, "\t(input file %s does not exist)\n", tFile);
                    }
                    else if(errnum == EISDIR){
                        fprintf(stdout, "\t(input file %s is a directory or line 1 is not just a number)\n", tFile);
                    }
                    exit(-1);
                }

                // read tsfile first line
                char *reader = NULL;
                size_t len = 0;
                ssize_t nread;
                if((nread = getline(&reader, &len, filereader)) != -1){
                    if(nread > 1024){
                        fprintf(stdout, "\t(malformed input - line 1 is not just a number)\n");
                        exit(-1);
                    }
                    if(checkNumber(reader) == FALSE){
                        fprintf(stdout, "\t(malformed input - line 1 is not just a number)\n");
                        exit(-1);
                    }
                    num = atoi(reader);
                }else{
                    fprintf(stdout, "\t(malformed input - line 1 is not just a number)\n");
                    exit(-1);
                }
                fclose(filereader);
            }
            else{
                fprintf(stdout, "\t(malformed command, \"%s\" is not a valid commandline option)\n", name);
                exit(-1);
            }


            i += 2;
        }
    }


    struct timeval now;
    struct timespec cur;
    cur.tv_nsec = 0;
    cur.tv_sec = 0;

    
    pthread_t packets;
    pthread_t tokens;
    pthread_t server1;
    pthread_t server2;
    pthread_t catchCntl;

    ThreadParams params;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    params.mutex = &mutex;
    pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
    params.cv = &cv;
    params.start = &now;
    params.tsfile = NULL;
    if(tFile != NULL){
        params.tsfile = fopen(tFile, "r");
    }
    params.B = &B;
    params.lambda = &lambda;
    params.mu = &mu;
    params.num = &num;
    params.P = &P;
    params.r = &r;

    My402List Q1;
    memset(&Q1, 0, sizeof(My402List));
    (void)My402ListInit(&Q1);
    My402List Q2;
    memset(&Q2, 0, sizeof(My402List));
    (void)My402ListInit(&Q2);
    params.Q1 = &Q1;
    params.Q2 = &Q2;

    int served_packet = 0;
    params.served_packets = &served_packet;
    params.num_drop = 0;

    // setup thread parameters
    ServerThreadParams sp1;
    sp1.index = 1;
    sp1.params = &params;
    ServerThreadResult sResult1;
    sResult1.total_service_time = (struct timeval *)malloc(sizeof(struct timeval));
    sResult1.total_stay_Q2 = (struct timeval *)malloc(sizeof(struct timeval));
    sResult1.total_system_spent = (struct timeval *)malloc(sizeof(struct timeval));
    sResult1.avg_of_timevalSquared = 0.0;
    sResult1.avg_packet_service_time = 0.0;
    sResult1.served_packets = 0;
    sResult1.avg_packet_spent_in_system = 0.0;
    sResult1.avg_squared_packet_spent_in_system = 0.0;
    sp1.result = &sResult1;
    ServerThreadParams sp2;
    sp2.index = 2;
    sp2.params = &params;
    ServerThreadResult sResult2;
    sResult2.total_service_time = (struct timeval *)malloc(sizeof(struct timeval));
    sResult2.total_stay_Q2 = (struct timeval *)malloc(sizeof(struct timeval));
    sResult2.total_system_spent = (struct timeval *)malloc(sizeof(struct timeval));
    sResult2.avg_of_timevalSquared = 0.0;
    sResult2.avg_packet_service_time = 0.0;
    sResult2.served_packets = 0;
    sResult2.avg_packet_spent_in_system = 0.0;
    sResult2.avg_squared_packet_spent_in_system = 0.0;
    sp2.result = &sResult2;

    PacketThreadParams pp;
    pp.params = &params;
    PacketThreadResult pResult;
    // pResult.inter_arrival_time_sum = (struct timeval *)malloc(sizeof(struct timeval));
    pResult.total = 0;
    pResult.avg_inter_arrival_sec = 0.0;
    pp.result = &pResult;

    TokenThreadParams tp;
    tp.params = &params;
    TokenThreadResult tResult;
    tResult.drop_tokens = 0;
    tResult.total_tokens = 0;
    tResult.total_stay_Q1 = (struct timeval *)malloc(sizeof(struct timeval));
    tp.result = &tResult;

    sigset_t set;
    int s;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);
    s = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if(s != 0){
        handle_error_en(s, "pthread_sigmask");
    }

    CancelThreadParams cp;
    cp.params = &params;
    cp.packetThread = &packets;
    cp.tokenThread = &tokens;
    cp.set = &set;
    pthread_create(&catchCntl, 0, CatchCntlThread, (void *) &cp);

    printf("%05ld%03ld.%03ldms: emulation begins\n", cur.tv_sec, cur.tv_nsec/1000000, cur.tv_nsec % 1000000);
    gettimeofday(&now, 0);
    pthread_create(&packets, 0, PacketArrivalThread, (void *)&pp);
    pthread_create(&tokens, 0, TokenDepositThread, (void *)&tp);
    pthread_create(&server1, 0, ServerThread, (void *)&sp1);
    pthread_create(&server2, 0, ServerThread, (void *)&sp2);

    pthread_join(packets, 0);
    pthread_join(tokens, 0);
    pthread_join(server1, 0);
    pthread_join(server2, 0);
    if(params.tsfile != NULL){
        fclose(params.tsfile);
    }

    struct timeval diff;
    struct timeval curr;
    gettimeofday(&curr, NULL);
    timevalBetween(&now, &curr, &diff);
    printf("%05ld%03ld.%03ldms: emulation ends\n", diff.tv_sec, diff.tv_usec/1000, diff.tv_usec % 1000);
    statistics(pp.result, pp.params->num_drop, tp.result, &diff, sp1.result, sp2.result, num);
    return 0;
}
