/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
int prefix(char* pre,char* str);
void exe_cd_command(char **argv,int cur_dir,char* origin_path);
void parsepipe(char** argv,char* first_pipe_command,char* after_pipe_commands);
void pipe_execution(char** first_pipe_argv,char* after_pipe_commands);

int main() 
{
    char cmdline[MAXLINE]; /* Command line */

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

/*recursively execute cd command
this can not affect parent process*/
void exe_cd_command(char **argv,int cur_dir,char* origin_path){
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
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int argc;            /*Number of Argument list execve*/
    int pipe_flag=0;     /*Is there pipe line?*/
    char first_pipe_command[MAXARGS]={'\0'}, after_pipe_commands[MAXARGS]={'\0'};

    strcpy(buf, cmdline);/* 혹시 argv를 바꾸더라도 child process는 영향 없다. */
    bg = parseline(buf, argv); 
    for(argc=0;argc<MAXARGS;++argc)
        if(argv[argc]==NULL) break;
    if(strchr(cmdline,'|'))
        pipe_flag=1;
    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, cd -> run in parent, other -> run
        if((pid=Fork()==0)){//child process
            /*consider pipe command*/
            if(pipe_flag){
                parsepipe(argv,first_pipe_command,after_pipe_commands);
                char* first_pipe_argv[MAXARGS];
                parseline(first_pipe_command,first_pipe_argv);
                pipe_execution(first_pipe_argv,after_pipe_commands);


            }

            Execvp(argv[0], argv);	//ex) /bin/ls ls -al &
        }

        /* Parent waits for foreground job to terminate */
        if (!bg){ 
            int status;
            Waitpid(pid,&status,0);
        }
        else//when there is background process!
            printf("%d %s", pid, cmdline);
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
        char* origin_path=getcwd(origin_path,MAXARGS);
        exe_cd_command(temp,1,origin_path);
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
    char buf[MAXARGS];
    strcpy(buf,after_pipe_commands);
    char* after_pipe_argv[MAXARGS];
    char after_first_pipe_cmd[MAXARGS]={'\0'},after_after_pipe_cmds[MAXARGS]={'\0'};
    parseline(buf,after_pipe_argv);
    parsepipe(after_pipe_argv,after_first_pipe_cmd,after_after_pipe_cmds);
    //2. make after_first_pipe_argv
    char buf2[MAXARGS];
    char* after_first_pipe_argv[MAXARGS];
    strcpy(buf2,after_first_pipe_cmd);
    parseline(buf2,after_first_pipe_argv);

    Waitpid(pid,&child_status,0);
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
    int stack[MAXARGS];
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
            //" "| is only exception
            if(*(quot_e+1)=='|'){
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
            //" "| is only exception
            if(*(quot_e+1)=='|'){
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

    return bg;
}
/* $end parseline */


