/*
 * 메인스레드는 connfd를 buf에 넣고
 * 서브스레드는 buf를 소비하는
 * Producer-Consumer Problem
 */
#ifndef __SBUF_
#define __SBUF_
#include "csapp.h"

typedef struct {
    int* buf; //Buffer array
    int n; //최대 buf의 길이
    int front; 
    int rear;
    sem_t mutex;
    sem_t slots; //사용가능한 칸의 개수
    sem_t items; //이미 사용된 칸의 개수
}sbuf_t;

void sbuf_init(sbuf_t* sp,int n);
void sbuf_deinit(sbuf_t* sp);
void sbuf_insert(sbuf_t* sp,int item);
int sbuf_remove(sbuf_t* sp);

#endif