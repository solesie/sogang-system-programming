/*
* client는 항상 한 줄의 요청만을 보낸다.
*/
#include "csapp.h"
#include "reqres.h"

//server: buf에 client로 부터의 요청을 저장한다.
ssize_t req(rio_t* rp,char* buf){
    ssize_t ret = Rio_readlineb(rp,buf,MAXLINE);
    return ret;
}
//server: buf를 client에 응답한다.
void res(int fd, char* buf){
    Rio_writen(fd,buf,MAXLINE);
}