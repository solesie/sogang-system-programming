/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128
#define MAXJOBS   300
typedef struct _job_t{
    int job_entry;//using e.g. : %job_entry
    char status;//['f' : fg], ['b' : bg], ['s' : stopped in bg]
    char commands[MAXLINE];
    pid_t pid;
}job_t;
job_t job_lists[MAXJOBS];
int job_lists_size=0;

//(explicitly waiting)parent가 현재 fore-ground job child를 wait하는지의 여부
//0인 경우는 child가 back-ground job으로 바뀌었거나, terminated 되었을때이다.
int wait_child_flag=1;

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
int prefix(char* pre,char* str);
void exe_cd_command(char **argv,int cur_dir,char* origin_path);
void parsepipe(char** argv,char* first_pipe_command,char* after_pipe_commands);
void pipe_execution(char** first_pipe_argv,char* after_pipe_commands);
void sig_chld_handler(int sig);
void sig_tstp_handler(int sig);
void sig_int_handler(int sig);
int deletejob(pid_t pid);
void addjob(pid_t pid,char status,char* cmdline);
void update_job_status(pid_t pid, char status);

int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    Signal(SIGCHLD,sig_chld_handler);
    Signal(SIGTSTP,sig_tstp_handler);
    Signal(SIGINT,sig_int_handler);
    
    while (1) {
        /* Read */
        printf("> ");                   
        fgets(cmdline, MAXLINE, stdin); 
        if (feof(stdin))
            exit(0);

        /* Evaluate */
        eval(cmdline);
    } 
}

/* 해당 피드가 없으면 0, 있으면 1을 반환한다. */
int deletejob(pid_t pid){
    for(int i=0;i<job_lists_size;++i){
        if(job_lists[i].pid==pid){
            //job_lists[i]를 삭제하고 뒤의 원소를 앞으로 당긴다.
            for(int j=i+1;j<job_lists_size;++j)
                job_lists[j-1]=job_lists[j];
            --job_lists_size;
            return 1;
        }
    }
    return 0;
}

void addjob(pid_t pid,char status,char* cmdline){
    //give job_entry
    int used[MAXJOBS];
    memset(used,0,sizeof(used));
    for(int i=0;i<job_lists_size;++i)
        used[job_lists[i].job_entry]=1;
    int job_entry;
    for(int i=1;i<MAXJOBS;++i)
        if(!used[i]){
            job_entry=i;
            break;
        }
    job_lists[job_lists_size].job_entry=job_entry;
    job_lists[job_lists_size].pid=pid;
    job_lists[job_lists_size].status=status;
    strcpy(job_lists[job_lists_size].commands,cmdline);
    ++job_lists_size;
}

void sig_chld_handler(int sig){
    int olderrno=errno;
    int child_status;
    sigset_t mask_all,prev_all;
    pid_t pid;

    Sigfillset(&mask_all);
    while((pid=waitpid(-1,&child_status,WNOHANG))>0){//모든 종료된 child를 reap한다.(signal can not be queued)
        Sigprocmask(SIG_BLOCK,&mask_all,&prev_all);
        for(int i=0;i<job_lists_size;++i){
            if(job_lists[i].pid==pid&&job_lists[i].status=='f'){//foreground인 경우
                wait_child_flag=0;
                break;
            }
            else if(job_lists[i].pid==pid&&job_lists[i].status=='b'){//background running인 경우
                Sio_puts("[");
                Sio_putl((long)job_lists[i].job_entry);
                Sio_puts("]   done   ");
                Sio_puts(job_lists[i].commands);
                Sio_puts(">");
            }
        }
        deletejob(pid);//pid와 일치하는 child를 job list에서 삭제한다.
        Sigprocmask(SIG_SETMASK,&prev_all,NULL);
    }
    if(pid<0&&errno!=ECHILD)//sig_child_handler가 먼저 작동했을 수도 있다.
        unix_error("waitpid error");
    errno=olderrno;
}
void sig_tstp_handler(int sig){
    int olderrno=errno;
    sigset_t mask_all,prev_all;
    Sigfillset(&mask_all);
    //job_list에서 fore-ground job을 찾아서, 그것의 pg를 없앤다.
    Sigprocmask(SIG_BLOCK,&mask_all,&prev_all);
    for(int i=0;i<job_lists_size;++i){
        if(job_lists[i].status=='f'){
            Sio_puts("suspended   ");
            Sio_puts(job_lists[i].commands);
            wait_child_flag=0;
            kill(-job_lists[i].pid,SIGTSTP);
            job_lists[i].status='s';//background에서 stop되게 함
            break;
        }
    }
    Sigprocmask(SIG_SETMASK,&prev_all,NULL);
    errno=olderrno;
}
void sig_int_handler(int sig){
    int olderrno=errno;
    sigset_t mask_all,prev_all;
    Sigfillset(&mask_all);
    //job_list에서 fore-ground job을 찾아서, 그것의 pg를 없앤다.
    Sigprocmask(SIG_BLOCK,&mask_all,&prev_all);
    for(int i=0;i<job_lists_size;++i){
        if(job_lists[i].status=='f'){
            wait_child_flag=0;
            kill(-job_lists[i].pid,SIGINT);
            deletejob(job_lists[i].pid);
            break;
        }
    }
    Sigprocmask(SIG_SETMASK,&prev_all,NULL);
    errno=olderrno;
}

void exe_jobs_command(){
    for(int i=0;i<job_lists_size;++i){
        printf("[%d]   ",job_lists[i].job_entry);
        switch(job_lists[i].status){
            case 'b':
                printf("running   %s",job_lists[i].commands);
                break;
            case 's':
                printf("suspended   %s",job_lists[i].commands);
                break;
        }
    }
}

/*Change a stopped background job to a running background job.*/
void exe_bg_command(char** argv){
    if(job_lists_size==0){
        printf("bg: no current job\n");
        return;
    }
    if(argv[1]!=NULL && (argv[1][0]!='%'||argv[1][1]=='\0') ){
        printf("bg: job not found: %s\n",argv[1]);
        return;
    }
    int stopped_job_entry=-1;
    if(argv[1]!=NULL){
        stopped_job_entry=atoi(&argv[1][1]);
        if(stopped_job_entry==0){
            printf("bg: job not found: %s\n",argv[1]);
            return;
        }
    }

    int exist_stopped_job_flag=0;
    int exist_such_job_flag=0;
    for(int i=job_lists_size-1;i>=0;--i){
        if(job_lists[i].status=='s'){
            stopped_job_entry= argv[1]==NULL ? job_lists[i].job_entry : stopped_job_entry;
            exist_stopped_job_flag=1;
            break;
        }
    }
    for(int i=0;i<job_lists_size;++i){
        if(job_lists[i].job_entry==stopped_job_entry){
            exist_such_job_flag=1;
            break;
        }
    }
    if(!exist_stopped_job_flag){
        printf("bg: job already in background\n");
        return;
    }
    if(!exist_such_job_flag){
        printf("bg: %%%d : no such job\n",stopped_job_entry);
        return;
    }
    
    for(int i=0;i<job_lists_size;++i){
        if(job_lists[i].job_entry==stopped_job_entry){
            kill(-job_lists[i].pid,SIGCONT);
            job_lists[i].status='b';
            printf("[%d]   continued   %s",stopped_job_entry,job_lists[i].commands);
            break;
        }
    }
}

/*Change a stopped/running background job to a running forefround job.*/
void exe_fg_command(char** argv){
    sigset_t prev;
    Sigprocmask(SIG_BLOCK,NULL,&prev);
    if(job_lists_size==0){
        printf("fg: no current job\n");
        return;
    }
    if(argv[1]!=NULL && (argv[1][0]!='%'||argv[1][1]=='\0') ){
        printf("fg: job not found: %s\n",argv[1]);
        return;
    }
    int job_entry=-1;
    if(argv[1]!=NULL){
        job_entry=atoi(&argv[1][1]);
        if(job_entry==0){
            printf("fg: job not found: %s\n",argv[1]);
            return;
        }
    }

    int exist_such_job_flag=0;
    for(int i=0;i<job_lists_size;++i){
        if(job_lists[i].job_entry==job_entry){
            job_entry= argv[1]==NULL ? job_lists[i].job_entry : job_entry;
            exist_such_job_flag=1;
            break;
        }
    }
    if(!exist_such_job_flag){
        printf("bg: %%%d : no such job\n",job_entry);
        return;
    }
    
    for(int i=0;i<job_lists_size;++i){
        if(job_lists[i].job_entry==job_entry){
            if(job_lists[i].status=='s'){
                kill(-job_lists[i].pid,SIGCONT);
                printf("[%d]   continued   %s",job_entry,job_lists[i].commands);
            }
            else if(job_lists[i].status=='b'){
                printf("[%d]   running   %s",job_entry,job_lists[i].commands);
            }
            job_lists[i].status='f';
            while(wait_child_flag){
                Sigsuspend(&prev);
            }
            Sigprocmask(SIG_BLOCK,NULL,&prev);
            wait_child_flag=1;
            break;
        }
    }
}

void exe_kill_command(char** argv){
    if(argv[1]=='\0'){
        printf("kill: not enough arguments\n");
        return;
    }
    if(argv[1]!=NULL && (argv[1][0]!='%'||argv[1][1]=='\0') ){
        printf("kill: not enough arguments\n");
        return;
    }
    int job_entry=-1;
    job_entry=atoi(&argv[1][1]);
    if(job_entry==0){
        printf("fg: job not found: %s\n",argv[1]);
        return;
    }

    int exist_such_job_flag=0;
    for(int i=0;i<job_lists_size;++i){
        if(job_lists[i].job_entry==job_entry){
            job_entry= argv[1]==NULL ? job_lists[i].job_entry : job_entry;
            exist_such_job_flag=1;
            break;
        }
    }
    if(!exist_such_job_flag){
        printf("kill: %%%d : no such job\n",job_entry);
        return;
    }
    
    for(int i=0;i<job_lists_size;++i){
        if(job_lists[i].job_entry==job_entry){
            kill(-job_lists[i].pid,SIGKILL);
            printf("[%d]   terminated   %s",job_entry,job_lists[i].commands);
            deletejob(job_lists[i].pid);
            break;
        }
    }
}

/*recursively execute cd command.
this can not affect parent process*/
void exe_cd_command(char** argv,int cur_dir,char* origin_path){
    if(argv[1]==NULL){
        chdir(getenv("HOME"));
        return;
    }
    if(argv[cur_dir]==NULL) 
        return;
    if(chdir(argv[cur_dir])<0){
        Sio_puts("cd: no such file or directory: ");
        for(int i=1;i<=cur_dir;++i){
            Sio_puts(argv[i]);
            Sio_puts("/");
        }
        Sio_puts("\n");
        chdir(origin_path);
        return;
    }
    exe_cd_command(argv,cur_dir+1,origin_path);
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
//SIGINT,SIGTSTP 블락된 상태
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int argc;            /*Number of Argument list execve*/
    int pipe_flag=0;     /*Is there pipe line?*/
    char first_pipe_command[MAXLINE]={'\0'}, after_pipe_commands[MAXLINE]={'\0'};

    sigset_t mask_all,mask_one,prev_one; // WITHOUT RACE
    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one,SIGCHLD);

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    for(argc=0;argc<MAXARGS;++argc)
        if(argv[argc]==NULL) break;
    if(strchr(cmdline,'|'))
        pipe_flag=1;
    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */

    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, cd -> run in parent, other -> run
        Sigprocmask(SIG_BLOCK,&mask_one,&prev_one);//SIGINT, SIGTSTP, SIGCHLD 블락
        if((pid=Fork())==0){ //child process
            Sigprocmask(SIG_SETMASK,&prev_one,NULL);//SIGINT, SIGTSTP 블락
            if(!bg){//fore-ground 인 경우 SIGINT, SIGTSTP도 받을 수 있다.
                Sigprocmask(SIG_UNBLOCK,&prev_one,NULL);
            }
            setpgid(0,0);//현재 process의 pid로 pgid를 설정한다.

            /*consider pipe command*/
            if(pipe_flag){
                parsepipe(argv,first_pipe_command,after_pipe_commands);
                char* first_pipe_argv[MAXARGS];
                parseline(first_pipe_command,first_pipe_argv);
                pipe_execution(first_pipe_argv,after_pipe_commands);
            }

            Execvp(argv[0], argv);	//ex) /bin/ls ls -al &
        }
        Sigprocmask(SIG_BLOCK,&mask_all,NULL);
        addjob(pid,bg?'b':'f',cmdline);
        Sigprocmask(SIG_SETMASK,&prev_one,NULL);//SIGINT, SIGTSTP 블락

        /* Parent waits for foreground job to terminate */
        if (!bg){ 
            int status;
            while(wait_child_flag){
                Sigsuspend(&prev_one);
            }
            Sigprocmask(SIG_SETMASK,&prev_one,NULL);//SIGINT, SIGTSTP 블락
            wait_child_flag=1;
        }
        else//when there is background process!
            printf("%d %s\n", pid, cmdline);
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    char** temp=argv;
    if(!strcmp(argv[0],"exit"))//exit command
        exit(0);
    if (!strcmp(argv[0], "quit"))//quit command 
	    exit(0);  
    if (!strcmp(argv[0], "&"))    //Ignore singleton & 
	    return 1;
    if(!strcmp(argv[0],"cd")){//because there isn't /bin/cd/ 
        char* origin_path=getcwd(origin_path,MAXLINE);
        exe_cd_command(temp,1,origin_path);
        return 1;
    }
    if(!strcmp(argv[0],"jobs")){
        exe_jobs_command();
        return 1;
    }
    if(!strcmp(argv[0],"bg")){
        exe_bg_command(temp);
        return 1;
    }
    if(!strcmp(argv[0],"fg")){
        exe_fg_command(temp);
        return 1;
    }
    if(!strcmp(argv[0],"kill")){
        exe_kill_command(temp);
        return 1;
    }
    return 0;                     /* Not a builtin command */
}
/* $end eval */

void pipe_execution(char** first_pipe_argv,char* after_pipe_commands){
    pid_t pid;
    int fd[2];
    int child_status;
    Pipe(fd);
    //Child process: Execute first_pipe_argv
    if((pid=Fork())==0){
        close(fd[0]);
        Dup2(fd[1],STDOUT_FILENO);
        close(fd[1]);
        Execvp(first_pipe_argv[0],first_pipe_argv);//output: inside of fd[1]
    }

    //Parent process: pipeparse(after_pipe_commands) and recursion
    close(fd[1]);
    Dup2(fd[0],STDIN_FILENO);
    close(fd[0]);

    //1. parse after_pipe_commands
    char buf[MAXBUF];
    strcpy(buf,after_pipe_commands);
    char* after_pipe_argv[MAXARGS];
    char after_first_pipe_cmd[MAXLINE]={'\0'},after_after_pipe_cmds[MAXLINE]={'\0'};
    parseline(buf,after_pipe_argv);
    parsepipe(after_pipe_argv,after_first_pipe_cmd,after_after_pipe_cmds);
    //2. make after_first_pipe_argv
    char buf2[MAXBUF];
    char* after_first_pipe_argv[MAXARGS];
    strcpy(buf2,after_first_pipe_cmd);
    parseline(buf2,after_first_pipe_argv);

    pid=waitpid(pid,&child_status,0);
    if(pid<0&&errno!=ECHILD)//sig_child_handler가 정말 우연치 않게 먼저 작동했을 수도 있다.
        unix_error("waitpid error");
    //base case: no pipe anymore
    if(!strchr(after_pipe_commands,'|'))
        Execvp(after_pipe_argv[0],after_pipe_argv);
    pipe_execution(after_first_pipe_argv,after_after_pipe_cmds);
}

/* Parse argv into first_command and after_commands */
void parsepipe(char** argv,char* first_pipe_command,char* after_pipe_commands){
    int i,j;
    int cur_fc_idx=0;
    int cur_pa_idx=0;
    for(i=0;argv[i]!=NULL;++i){
        if(strchr(argv[i],'|')){
            for(j=0;argv[i][j]!='|';++j,++cur_fc_idx){
                first_pipe_command[cur_fc_idx]=argv[i][j];
            }
            ++j;
            first_pipe_command[cur_fc_idx++]='\n';
            first_pipe_command[cur_fc_idx]='\0';
            
            //Initialize after_pipe_commands and return
            for(;argv[i]!=NULL;++i){
                for(;argv[i][j]!='\0';++j,++cur_pa_idx){
                    after_pipe_commands[cur_pa_idx]=argv[i][j];
                }
                after_pipe_commands[cur_pa_idx++]=' ';
                j=0;
            }
            after_pipe_commands[cur_pa_idx]='\n';
            after_pipe_commands[cur_pa_idx]='\0';
            break;
        }
        for(j=0;argv[i][j]!='\0';++j,++cur_fc_idx){
            first_pipe_command[cur_fc_idx]=argv[i][j];
        }
        first_pipe_command[cur_fc_idx++]=' ';
    }
}

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */
    int double_quot_flag=0,single_quot_flag=0;
    int quot_flag=0;
    char* temp=buf;/*To save char temporary*/

    /* Replace trailing '\n' with space */
    buf[strlen(buf)-1] = ' ';
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	    buf++;
    
    /* Build the argv list */
    /*consider quotation(jump)*/
    int stack[MAXLINE];
    memset(stack,-1,sizeof(stack));
    int stack_size=0;
    for(int i=0;i<strlen(buf);++i){
        if(buf[i]==34){// 34: " 
            if(stack_size>0 && stack[stack_size-1]==34)//pop
                --stack_size;
            else{//push
                stack[stack_size]=34;
                ++stack_size;
            }
        }
        else if(buf[i]==39){// 39: ' 
            if(stack_size>0 && stack[stack_size-1]==39)//pop
                --stack_size;
            else{//push
                stack[stack_size]=39;
                ++stack_size;
            }
        }
    }
    quot_flag=stack_size==0 ? 1 : 0;
    
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        temp=buf;
        char* quot_s=buf,*quot_e=buf;
        if(quot_flag&&(*buf==34||*buf==39))
            ++buf;
        argv[argc++] = buf;
        if(*temp == 34&&quot_flag){
            quot_s=temp;
            ++temp;
            quot_e=strchr(temp,34);
            *quot_e='\0';
            //" "|, " "& are exception
            if(*(quot_e+1)=='|'||*(quot_e+1)=='&'){
                buf=quot_e+1;
                argv[argc++]=buf;
            }
            temp=quot_e+1;
            delim=strchr(temp,' ');
        }
        else if(*temp == 39&&quot_flag){
            quot_s=temp;
            ++temp;
            quot_e=strchr(temp,34);
            *quot_e='\0';
            //" "|, " "& are exception
            if(*(quot_e+1)=='|'||*(quot_e+1)=='&'){
                buf=quot_e+1;
                argv[argc++]=buf;
            }
            temp=quot_e+1;
            delim=strchr(temp,' ');
        }
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	    return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	    argv[--argc] = NULL;
    else{
        int idx=strlen(argv[argc-1])-1;
        if((bg=(argv[argc-1][idx]=='&'))!=0)
            argv[argc-1][idx]=NULL;
    }
    

    return bg;
}
/* $end parseline */


