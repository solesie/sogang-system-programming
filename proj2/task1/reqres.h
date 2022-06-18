/*
* client는 항상 한 줄의 요청만을 보낸다.
*/
#ifndef __REQRES_
#define __REQRES_
#include "csapp.h"

//server: buf에 client로 부터의 요청을 저장한다.
ssize_t req(rio_t* rp,char* buf);
//server: buf를 client에 응답한다.
void res(int fd, char* buf);
#endif