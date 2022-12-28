#ifndef _RECORD_H_
#define _RECORD_H_

#include "cs402.h"
#include "my402list.h"

typedef struct Record{
    char type;
    unsigned int time;
    int amount;
    char description[1024];
} Record;

typedef struct RecordList{
    My402List list;
} RecordList;

extern void printRecord(Record * record);

extern int initList(RecordList * list);

extern int addToList(RecordList * list, Record * record);

extern int sortList(RecordList * list);

extern int createRecord(Record * record, char type, unsigned int time, int amount, char * description);

extern void sortRecordList(My402List * list);

extern int compareRecord(Record * rec1, Record * rec2);

#endif /*_RECORD_H_*/