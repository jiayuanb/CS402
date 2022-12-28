#include "my402list.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <malloc.h>

int  My402ListLength(My402List* list){
    if(list == NULL){
        return 0;
    }
    return list -> num_members;
}

int  My402ListEmpty(My402List* list){
    if(list == NULL){
        return 0;
    }
    if(list->num_members > 0){
        return 0;
    }
    else{
        return 1;
    }
}

int  My402ListAppend(My402List* list, void* obj){
    if(list == NULL){
        return 0;
    }
    My402ListElem * last = My402ListLast(list);
    if(last == NULL){
        last = &(list->anchor);
    }
    
    My402ListElem * newElem = (My402ListElem *) malloc(sizeof(My402ListElem));
    newElem->obj = obj;
    last->next = newElem;
    newElem->next = &(list->anchor);
    newElem->prev = last;
    list->anchor.prev = newElem;
    (list->num_members)++;
    return 1;
}

int  My402ListPrepend(My402List* list, void* obj){
    if(list == NULL){
        return 0;
    }
    My402ListElem * first = My402ListFirst(list);
    if(first == NULL){
        first = &(list->anchor);
    }
    My402ListElem * newItem = (My402ListElem *) malloc(sizeof(My402ListElem));
    newItem->obj = obj;
    newItem->next = first;
    newItem->prev = &(list->anchor);
    first->prev = newItem;
    list->anchor.next = newItem;
    (list->num_members)++;
    return 1;
}

void My402ListUnlink(My402List* list, My402ListElem* elem){
    My402ListElem * prev = elem->prev;
    My402ListElem * next = elem->next;
    if(prev != NULL){
        prev->next = next;
    }
    if(next != NULL){
        next->prev = prev;
    }
    elem->prev = NULL;
    elem->next = NULL;
    (list->num_members)--;
}

void My402ListUnlinkAll(My402List* list){
    My402ListElem * start = list->anchor.next;
    while(start != &(list->anchor)){
        My402ListElem * nextNode = start->next;
        My402ListUnlink(list, start);
        start = nextNode;
    }
    list->num_members = 0;
}

int  My402ListInsertAfter(My402List* list, void* obj, My402ListElem* elem){
    if(list == NULL){
        return 0;
    }
    if(elem == NULL){
        return My402ListAppend(list, obj);
    }
    My402ListElem * nextNode = elem->next;
    My402ListElem * newNode = (My402ListElem *) malloc(sizeof(My402ListElem));
    newNode->obj = obj;
    elem->next = newNode;
    newNode->next = nextNode;
    nextNode->prev = newNode;
    newNode->prev = elem;
    list->num_members++;
    return 1;
}

int  My402ListInsertBefore(My402List* list, void* obj, My402ListElem* elem){
    if(list == NULL){
        return 0;
    }
    if(elem == NULL){
        return My402ListPrepend(list, obj);
    }
    My402ListElem * prevNode = elem->prev;
    My402ListElem * newNode = (My402ListElem *) malloc(sizeof(My402ListElem));
    newNode->obj = obj;
    prevNode->next = newNode;
    newNode->next = elem;
    elem->prev = newNode;
    newNode->prev = prevNode;
    list->num_members++;
    return 1;
}


My402ListElem *My402ListFirst(My402List* list){
    if(My402ListEmpty(list)){
        return NULL;
    }
    else{
        return (list->anchor).next;
    }
}

My402ListElem *My402ListLast(My402List* list){
    if(My402ListEmpty(list)){
        return NULL;
    }
    else{
        return (list->anchor).prev;
    }
}

My402ListElem *My402ListNext(My402List* list, My402ListElem* elem){
    My402ListElem * end = &(list -> anchor);
    // get next
    My402ListElem * nextElem = elem->next;
    if(nextElem == end){
        // both end and nextElem have the same address stored
        return NULL;
    }
    else{
        return nextElem;
    }
}

My402ListElem *My402ListPrev(My402List* list, My402ListElem* elem){
    My402ListElem * start = &(list -> anchor);
    My402ListElem * prevElem = elem->prev;
    if(start == prevElem){
        return NULL;
    }
    else{
        return prevElem;
    }
}


My402ListElem *My402ListFind(My402List* list, void* obj){
    My402ListElem * start = &(list->anchor);
    while((start->next) != &(list->anchor)){
        if(start->next->obj == obj){
            return start->next;
        }
        start = start->next;
    }
    return NULL;
}


int My402ListInit(My402List* list){
    // check if enough memory has been allocated
    if(list == NULL){
        return 0;
    }
    // size_t listSize = malloc_usable_size(list);
    // if(listSize < sizeof(My402List)){
    //     return 0;
    // }
    // check if anchor can be allocated
    My402ListElem * anchor = (My402ListElem *)malloc(sizeof(My402ListElem));
    if(anchor == NULL){
        return 0;
    }
    anchor->next = anchor;
    anchor->prev = anchor;
    anchor->obj = list;
    list->anchor = (*anchor);
    list->num_members = 0;
    return 1;
}

