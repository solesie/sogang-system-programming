#include "csapp.h"
#include "reqres.h"
#include "treap.h"
#include "sbuf.h"
#include <errno.h>
#define NTHREADS 30
#define MAX_CLIENT 100

int byte_cnt;/*counts total bytes received by server*/
sem_t mutex_byte_cnt;
Node* root=NULL;
sbuf_t sbuf; /*Shared buffer of connected descriptors*/

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

static void init_carryout_client_cmd(void){
    byte_cnt=0;
    Sem_init(&mutex_byte_cnt,0,1);

    read_tree=0;
    Sem_init(&mutex_read_tree,0,1);
    Sem_init(&w_tree,0,1);
}
/*
 * thread safe 해야한다.
 * 다른 스레드가 treap을 수정하는 동안은 어떤 누구도 읽고 쓸수가 없다.(sell, buy)
 * 읽기(show)는 아무 스레드든 수행하면 된다.
 * Readers-Writers Problem
 */
void carryout_client_cmd(int connfd){
    int n;
    char buf[MAXLINE];
    char echo_with_res[MAXLINE];
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    Pthread_once(&once,init_carryout_client_cmd);
    Rio_readinitb(&rio,connfd);
    while((n = req(&rio, buf)) != 0){
        strcpy(echo_with_res,buf);
        // byte_cnt critical section start */
        P(&mutex_byte_cnt);
        byte_cnt+=n;
        printf("Server received %d (%d total) bytes on fd %d\n",n,byte_cnt,connfd);
        V(&mutex_byte_cnt);
        // byte_cnt critical section end
        //buf요청에 대한 작업을 실행한다.
        char temp[MAXLINE];
        strcpy(temp,buf);
        char* parsed_req[4];
        int valid=parseline(temp,parsed_req,connfd);
        if(valid&&!strcmp(parsed_req[0],"show")){
            char stock_info[MAXLINE]="";
            
            P(&mutex_read_tree);
            ++read_tree;
            if(read_tree==1) //First in
                P(&w_tree);
            V(&mutex_read_tree);
            print_in_buf(root,stock_info); //reading of tree
            P(&mutex_read_tree);
            --read_tree;
            if(read_tree==0) //Last out
                V(&w_tree); //기다리는 writers를 깨운다.
            V(&mutex_read_tree);

            strcat(echo_with_res,stock_info);
            res(connfd,echo_with_res);
        }
        else if(valid&&!strcmp(parsed_req[0],"buy")){ //writers of tree
            P(&w_tree);
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
            V(&w_tree);
        }
        else if(valid&&!strcmp(parsed_req[0],"sell")){ //writers of tree
            P(&w_tree);
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
            V(&w_tree);
        }
        else if(valid&&!strcmp(parsed_req[0],"exit")){
            break;
        }
        //유효한 요청이 아닌 경우
        else{
            strcpy(buf,"invalid request\n");
            strcat(echo_with_res,buf);
            res(connfd,echo_with_res);
        }
    }
}

void* thread(void* vargp){
    Pthread_detach(pthread_self());
    while(1){
        int connfd=sbuf_remove(&sbuf); //consumer
        carryout_client_cmd(connfd);
        Close(connfd);
    }
}

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

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
    sbuf_init(&sbuf,MAX_CLIENT);
    for(int i=0;i<NTHREADS;++i) //create worker threads
        Pthread_create(&tid,NULL,thread,NULL);
    while(1){
        clientlen = sizeof(struct sockaddr_storage); 
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //producer
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
            client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        sbuf_insert(&sbuf,connfd);
    }
    exit(0);
}
