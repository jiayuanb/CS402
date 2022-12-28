#include "record.h"
#include "my402list.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <malloc.h>
#include<time.h>


char * readNextUntilTab(char ** start_ptr){
    char *tab_ptr = strchr(*start_ptr, '\t');
    if (tab_ptr != NULL) {
        *tab_ptr++ = '\0';
    } else {
        return NULL;
    }
    return tab_ptr;
}

int ParseType(char ** start_ptr, Record * record){
    char * tab_ptr = readNextUntilTab(start_ptr);
    if(tab_ptr == NULL){return FALSE;}
    if(strlen(*start_ptr) != '1' && **start_ptr != '+' && **start_ptr != '-'){
        return FALSE;
    }
    record->type = **start_ptr;
    *start_ptr = tab_ptr;
    return TRUE;
}

int ParseTimestamp(char ** start_ptr, Record * record){
    
    char *tab_ptr = readNextUntilTab(start_ptr);
    if(tab_ptr == NULL){return FALSE;}
    if(**start_ptr == ' ' || **start_ptr == '0'){
        return FALSE;
    }
    if(strlen(*start_ptr) >= 11){
        return FALSE;
    }
    record->time = strtoul(*start_ptr, &tab_ptr, 10);
    if(record->time <= 0 || record->time >= (unsigned)time(NULL)){
        return FALSE;
    }
    tab_ptr++;
    *start_ptr = tab_ptr;
    return TRUE;
}

int ParseAmount(char ** start_ptr, Record * record){
    char *tab_ptr = readNextUntilTab(start_ptr);
    if(tab_ptr == NULL){return FALSE;}
    int amount = 0;
    char * start = *(start_ptr);
    while(start != tab_ptr){
        if(*start <= '9' && *start >= '0'){
            amount = amount * 10 + (*start) - '0';
        }
        start++;
    }
    record->amount = amount;
    *start_ptr = tab_ptr;
    return TRUE;
}

int ParseLine(char * buf, Record * record){
    char *start_ptr = buf;
    if(!ParseType(&start_ptr, record)){
        return FALSE;
    }
    // parse the timestamp
    if(!ParseTimestamp(&start_ptr, record)){
        return FALSE;
    }
    // parse the amount
    if(!ParseAmount(&start_ptr, record)){
        return FALSE;
    }
    // parse the description
    while(*start_ptr == ' '){
        start_ptr++;
    }
    char *tab_ptr = strchr(start_ptr, '\t');
    if (tab_ptr != NULL) {
        return FALSE;
    }
    tab_ptr = strchr(start_ptr, '\n');
    int desc_length = strlen(start_ptr);
    if(tab_ptr != NULL){
        desc_length = tab_ptr - start_ptr;
    }
    if(desc_length == 0){
        return FALSE;
    }
    memcpy(record->description, start_ptr, desc_length);
    *(record->description + desc_length) = '\0';
    return TRUE;
}


int ReadInput(FILE * fp, My402List * list){
    if(fp == NULL || list == NULL){
        return FALSE;
    }
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        if(strnlen(buf, 1024) == 0){
            return FALSE;
        }
        Record * record = (Record *) malloc(sizeof(Record));
        if(ParseLine(buf, record) == FALSE){
            return FALSE;
        }
        My402ListAppend(list, record);
    }
    return TRUE;
}

void printStatement(My402List * pList){
    My402ListElem *elem=NULL;
    for (elem=My402ListFirst(pList); elem != NULL; elem=My402ListNext(pList, elem)) {
        Record * ival=(Record *)(elem->obj);

        printRecord(ival);
    }
    fprintf(stdout, "\n");
}

void OutputSplitLine(){
    fprintf(stdout, "+-----------------+--------------------------+----------------+----------------+\n");
}

void OutputTitle(){
    fprintf(stdout, "|       Date      | Description              |         Amount |        Balance |\n");
}

void OutputRecord(char * date, char * desc, char * amount, char * balance){
    fprintf(stdout, "| %s | %s | %s | %s |\n", date, desc, amount, balance);
}

void itoa(int number, char * buffer, int base){
    int isNegative = FALSE;
    if(number < 0){
        isNegative = TRUE;
        number = - number;
    }
    int index = 0;
    while(number > 0){
        int residue = number % base;
        buffer[index] = residue > 9 ? (residue - 10 + 'a') : (residue + '0');
        index++;
        number = number / base;
    }
    if(isNegative){
        buffer[index++] = '-';
    }
    buffer[index] = '\0';
    int i=0;
    int j=index - 1;
    while(i < j){
        char temp = buffer[i];
        buffer[i] = buffer[j];
        buffer[j] = temp;
        i++;
        j--;
    }
}

int AmountToStr(int number, char * amount, char type){
    int amountIndex = strlen(amount);
    char buffer[33];
    if(type == '-'){
        amount[amountIndex - 1] = ')';
        amount[0] = '(';
    }
    amountIndex--;
    itoa(number, buffer, 10);
    int bufIndex = strlen(buffer);
    if(bufIndex < 2){
        memcpy(amount + amountIndex - bufIndex, buffer, bufIndex);
        memset(amount + amountIndex - 2, '0', 2 - bufIndex);
        *(amount + amountIndex - 3) = '.';
        *(amount + amountIndex - 4) = '0';
    }
    memcpy(amount + amountIndex - 2, buffer + bufIndex - 2, 2);
    amountIndex -= 2;
    bufIndex -= 2;
    *(amount + amountIndex - 1) = '.';
    amountIndex--;
    
    if(bufIndex <= 0){
        *(amount + amountIndex - 1) = '0';
        return TRUE;
    }
    while(bufIndex > 2){
        memcpy(amount + amountIndex - 3, buffer + bufIndex - 3, 3);
        amountIndex -= 3;
        bufIndex -= 3;
        if(bufIndex == 0){
            return TRUE;
        }
        *(amount + amountIndex - 1) = ',';
        amountIndex--;
    }
    memcpy(amount + amountIndex - bufIndex, buffer, bufIndex);
    return TRUE;
}

void OutputRecords(My402List * pList){
    My402ListElem *elem=NULL;
    OutputSplitLine();
    OutputTitle();
    OutputSplitLine();
    int balance = 0;
    for (elem=My402ListFirst(pList); elem != NULL; elem=My402ListNext(pList, elem)) {
        Record * ival=(Record *)(elem->obj);

        time_t ts = ival->time;
        struct tm lt = *localtime(&ts);
        char date[16];
        strftime(date, 16, "%a %b %e %Y", &lt);
        char description[25];
        memset(description, ' ', sizeof(description));
        int length = strnlen(ival->description, 24);
        memcpy(description, ival->description, length);
        description[24] = '\0';
        char amount[15];
        memset(amount, ' ', 14);
        amount[14] = '\0';
        if(!AmountToStr(ival->amount, amount, ival->type)){
            fprintf(stderr, "cannot translate amount into number");
            exit(-1);
        }
        if(ival->type == '-'){
            balance -= ival->amount;
        }
        else if(ival->type == '+'){
            balance += ival->amount;
        }
        else{
            fprintf(stderr, "wrong type");
            exit(-1);
        }
        char curBalance[15];
        memset(curBalance, ' ', 14);
        curBalance[14] = '\0';
        if(balance > 0){
            AmountToStr(balance, curBalance, '+');
        }
        else{
            AmountToStr(-balance, curBalance, '-');
        }
        OutputRecord(date, description, amount, curBalance);
    }
    OutputSplitLine();
}

int main(int argc, char const *argv[])
{
    FILE* handler = NULL;
    if(argc == 3){
        if(strcmp(argv[1], "sort") != 0){
            fprintf(stderr, "cannot read first argv %s\n", argv[1]);
            exit(-1);
        }
        handler = fopen(argv[2], "r");
    } else if(argc == 2){
        if(strcmp(argv[1], "sort") != 0){
            fprintf(stderr, "cannot read first argv %s", argv[1]);
            exit(-1);
        }
        handler = stdin;
    } else{
        fprintf(stderr, "wrong argc");
        exit(-1);
    }

    if(handler == NULL){
        fprintf(stderr, "no input is identified");
        exit(-1);
    }
    My402List list;
    memset(&list, 0, sizeof(My402List));
    if(!My402ListInit(&list)){
        fprintf(stderr, "cannot initialize My302List");
        exit(-1);
    }
    if(!ReadInput(handler, &list)){
        fprintf(stderr, "cannot read input");
        exit(-1);
    }
    if (handler != stdin) fclose(handler);
    sortRecordList(&list);
    OutputRecords(&list);
    return 0;
}
