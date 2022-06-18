/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#include "reqres.h"
#include "treap.h"

void echo(int connfd);
typedef struct{/*represents a pool of connected descriptors*/
    int maxfd; /*largest descriptor in read_set*/
    fd_set read_set;/*set of all active descriptors*/
    fd_set ready_set;/*읽기 준비가 완료된 subset of descriptors*/
    int nready;/*num of ready descriptors from select*/
    int maxi;/*client array에서 제일 큰 index*/
    int clientfd[FD_SETSIZE];/*set of active descriptors*/
    rio_t clientrio[FD_SETSIZE]/*set of active read buffers*/

}pool;

int byte_cnt=0;/*counts total bytes received by server*/
Node* root=NULL;

void sig_int_handler(int sig){
    int olderrno=errno;
    sigset_t mask_all,prev_all;
    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK,&mask_all,&prev_all);
    update_stock_txt(root);
    Sigprocmask(SIG_SETMASK,&prev_all,NULL);
    errno=olderrno;
    exit(0);
}

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv,int connfd) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        if(argc>=3)
            return 0;
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    if(( (argc==1&&!strcmp(argv[0],"show")) || (argc==3&&!strcmp(argv[0],"buy")) 
        || (argc==3&&!strcmp(argv[0],"sell")) || (argc==1&&!strcmp(argv[0],"exit")) ))
        return 1;
    else
        return 0;
}
/* $end parseline */

void init_pool(int listenfd, pool* p){
    p->maxi=-1;
    for(int i=0;i<FD_SETSIZE;++i)
        p->clientfd[i]=-1;
    p->maxfd=listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd,&p->read_set);
}
void add_client(int connfd,pool* p){
    int i;
    p->nready--;//listenfd 제외
    for(i=0;i<FD_SETSIZE;++i){//find available slot
        if(p->clientfd[i]<0){
            //pool에 connfd를 추가한다.
            p->clientfd[i]=connfd;
            Rio_readinitb(&p->clientrio[i],connfd);

            //connfd를 descriptor set에 추가한다.
            FD_SET(connfd,&p->read_set);

            //maxi와 maxfd를 update한다.
            if(connfd>p->maxfd)
                p->maxfd=connfd;
            if(i>p->maxi)
                p->maxi=i;
            break;
        }
    }
    //접속량 초과
    if(i==FD_SETSIZE)
        app_error("add_client error: Too many clients");
}
void check_clients(pool* p){
    int i,connfd,n;
    char buf[MAXLINE];
    char echo_with_res[MAXLINE];
    rio_t rio;
    for(int i=0;(i<=p->maxi)&&(p->nready>0);++i){
        connfd=p->clientfd[i];
        rio=p->clientrio[i];

        //connfd가 준비가 되었다면, 요청을 받고, 응답을 보낸다.
        if((connfd>0)&&(FD_ISSET(connfd,&p->ready_set))){
            p->nready--;
            if((n = req(&rio, buf)) != 0){
                strcpy(echo_with_res,buf);
                byte_cnt+=n;
                printf("Server received %d (%d total) bytes on fd %d\n",n,byte_cnt,connfd);
                //buf요청에 대한 작업을 실행한다.
                char temp[MAXLINE];
                strcpy(temp,buf);
                char* parsed_req[4];
                int valid=parseline(temp,parsed_req,connfd);
                if(valid&&!strcmp(parsed_req[0],"show")){
                    char stock_info[MAXLINE]="";
                    print_in_buf(root,stock_info);
                    strcat(echo_with_res,stock_info);
                    res(connfd,echo_with_res);
                }
                else if(valid&&!strcmp(parsed_req[0],"buy")){
                    int key=atoi(parsed_req[1]);
                    int buy_count=atoi(parsed_req[2]);
                    Node* node=find_key(root,key);
                    if(node==NULL){
                        strcpy(buf,"no such stock\n");
                        strcat(echo_with_res,buf);
                        res(connfd,echo_with_res);
                    }
                    else{
                        if(node->left_stock<buy_count){
                            strcpy(buf,"Not enough left stock\n");
                            strcat(echo_with_res,buf);
                            res(connfd,echo_with_res);
                        }
                        else{
                            node->left_stock-=buy_count;
                            strcpy(buf,"[buy] success\n");
                            strcat(echo_with_res,buf);
                            res(connfd,echo_with_res);
                        }
                    }
                }
                else if(valid&&!strcmp(parsed_req[0],"sell")){
                    int key=atoi(parsed_req[1]);
                    int sell_count=atoi(parsed_req[2]);
                    Node* node=find_key(root,key);
                    if(node==NULL){
                        strcpy(buf,"no such stock\n");
                        strcat(echo_with_res,buf);
                        res(connfd,echo_with_res);
                    }
                    else{
                        node->left_stock+=sell_count;
                        strcpy(buf,"[sell] success\n");
                        strcat(echo_with_res,buf);
                        res(connfd,echo_with_res);
                    }
                }
                else if(valid&&!strcmp(parsed_req[0],"exit")){
                    Close(connfd);
                    FD_CLR(connfd,&p->read_set);
                    p->clientfd[i]=-1;
                }
                //유효한 요청이 아닌 경우
                else{
                    strcpy(buf,"invalid request\n");
                    strcat(echo_with_res,buf);
                    res(connfd,echo_with_res);
                }
            }
            //클라이언트의 연결 종료로 eof가 반환된 경우
            else{
                Close(connfd);
                FD_CLR(connfd,&p->read_set);
                p->clientfd[i]=-1;
            }
        }
    }
}

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;
    Signal(SIGINT,sig_int_handler);

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    /*binary tree 초기화*/
    FILE* stock_txt=fopen("stock.txt","r");
    if(stock_txt==NULL){
        fclose(stock_txt);
        fprintf(stderr,"there is no stock.txt\n");
        exit(0);
    }
    char buf[MAXLINE];
    while(Fgets(buf,MAXLINE,stock_txt)!=NULL){
        Node* node=(Node*)malloc(sizeof(Node));
        int key,left_stock,price;
        sscanf(buf,"%d%d%d",&key,&left_stock,&price);
        init_node(node,key,left_stock,price);
        root=insert(root,node);
    }
    fclose(stock_txt);
    /*binary tree 초기화 완료*/

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd,&pool);
    while (1) {
        pool.ready_set=pool.read_set;
        //listenfd 혹은 connfd에 신호가 올때까지 기다린다.
        pool.nready=Select(pool.maxfd+1,&pool.ready_set,NULL,NULL,NULL);
        if(FD_ISSET(listenfd,&pool.ready_set)){
            clientlen = sizeof(struct sockaddr_storage); 
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
            add_client(connfd,&pool);
        }
        //연결된 클라이언트가 요청을 보내면 작업을 수행한다.
        check_clients(&pool);
    }
    exit(0);
}
/* $end echoserverimain */
