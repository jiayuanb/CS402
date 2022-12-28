#include "record.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <malloc.h>

int createRecord(Record * record, char type, unsigned int time, int amount, char * description){
    if(record == NULL){
        return FALSE;
    }
    record->type = type;
    record->time = time;
    record->amount = amount;
    size_t desc_size = sizeof(description);
    if(desc_size){
        return FALSE;
    }
    int startIndex = 0;
    while(startIndex < desc_size && *(description + startIndex) == ' '){
        startIndex++;
    }
    if(desc_size - startIndex == 0){
        return FALSE;
    }
    if(record->description == NULL){
        return FALSE;
    }
    memcpy(record->description, description + startIndex, desc_size - startIndex);
    return TRUE;
}

int initList(RecordList * list){
    if(list == NULL){
        return FALSE;
    }
    memset(&(list->list), 0, sizeof(My402List));
    return TRUE;
}

int addToList(RecordList * list, Record * record){
    if(list == NULL){
        return FALSE;
    }
    return My402ListAppend(&(list->list), record);
}

void printRecord(Record * record){
    if(record == NULL){
        fprintf(stderr, "record is NULL");
        exit(-1);
    }
    fprintf(stdout, "%c %u %d %s\n", record->type, record->time, record->amount, record->description);
}

int compareRecord(Record * rec1, Record * rec2){
    if(rec1 == NULL || rec2 == NULL){
        fprintf(stderr, "record is empty");
        exit(-1);
    }
    return rec1->time > rec2->time ? 1 : -1;
}

void BubbleForward(My402List *pList, My402ListElem **pp_elem1, My402ListElem **pp_elem2)
    /* (*pp_elem1) must be closer to First() than (*pp_elem2) */
{
    My402ListElem *elem1=(*pp_elem1), *elem2=(*pp_elem2);
    void *obj1=elem1->obj, *obj2=elem2->obj;
    My402ListElem *elem1prev=My402ListPrev(pList, elem1);
/*  My402ListElem *elem1next=My402ListNext(pList, elem1); */
/*  My402ListElem *elem2prev=My402ListPrev(pList, elem2); */
    My402ListElem *elem2next=My402ListNext(pList, elem2);

    My402ListUnlink(pList, elem1);
    My402ListUnlink(pList, elem2);
    if (elem1prev == NULL) {
        // printf("running prepend");
        (void)My402ListPrepend(pList, obj2);
        *pp_elem1 = My402ListFirst(pList);
    } else {
        (void)My402ListInsertAfter(pList, obj2, elem1prev);
        *pp_elem1 = My402ListNext(pList, elem1prev);
    }
    if (elem2next == NULL) {
        // printf("running append");
        (void)My402ListAppend(pList, obj1);
        *pp_elem2 = My402ListLast(pList);
    } else {
        (void)My402ListInsertBefore(pList, obj1, elem2next);
        *pp_elem2 = My402ListPrev(pList, elem2next);
    }
}

void sortRecordList(My402List * pList){
    My402ListElem *elem=NULL;
    int i=0;
    int num_items = pList->num_members;
    for (i=0; i < num_items; i++) {
        int j=0, something_swapped=FALSE;
        My402ListElem *next_elem=NULL;

        for (elem=My402ListFirst(pList), j=0; j < num_items-i-1; elem=next_elem, j++) {
            Record * cur_val=(Record *)(elem->obj);
            Record * next_val=NULL;

            next_elem=My402ListNext(pList, elem);
            next_val = (Record *)(next_elem->obj);

            if (compareRecord(cur_val, next_val) > 0) {
                // printf("cur: %d, next: %d", cur_val, next_val);
                BubbleForward(pList, &elem, &next_elem);
                something_swapped = TRUE;
            }
            // PrintTestList(pList, num_items);
        }
        if (!something_swapped) break;
        // PrintTestList(pList, num_items);
    }
}