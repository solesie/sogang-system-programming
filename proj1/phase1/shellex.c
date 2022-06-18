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

/*return whether str starts with pre*/
int prefix(char* pre,char* str){
    return strncmp(pre,str,strlen(pre))==0;
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
    char bin_path[MAXARGS]="/bin/";
    int pipe_flag=0;     /*Is there pipe line?*/
    

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    for(argc=0;argc<MAXARGS;++argc)
        if(argv[argc]==NULL) break;

    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, cd -> run in parent, other -> run
        /*if argv[0] isn't starts with '/bin/', concatenate them together*/
        if(!prefix(bin_path,argv[0]))
            argv[0]=strcat(bin_path,argv[0]);
        if((pid=Fork()==0)){/*child process*/
            /*consider pipe command*/
            

            Execve(argv[0], argv, environ);	//ex) /bin/ls ls -al &
        }

        /* Parent waits for foreground job to terminate */
        if (!bg){ 
            int status;
            Wait(&status);
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
    if(!strcmp(argv[0],"exit"))/*exit command*/
        exit(0);
    if (!strcmp(argv[0], "quit")) /* quit command */
	    exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	    return 1;
    if(!strcmp(argv[0],"cd")){/*because there isn't /bin/cd/ */
        char* origin_path=getcwd(origin_path,MAXARGS);
        exe_cd_command(temp,1,origin_path);
        return 1;
    }
    return 0;                     /* Not a builtin command */
}
/* $end eval */

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

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	    buf++;
    
    /* Build the argv list */
    /*consider quotation(jump)*/
    int stack[MAXARGS];
    memset(stack,-1,sizeof(stack));
    int stack_size=0;
    for(int i=0;i<strlen(buf);++i){
        if(buf[i]==34){/* 34: " */
            if(stack_size>0 && stack[stack_size-1]==34)/*pop*/
                --stack_size;
            else{/*push*/
                stack[stack_size]=34;
                ++stack_size;
            }
        }
        else if(buf[i]==39){/* 39: ' */
            if(stack_size>0 && stack[stack_size-1]==39)/*pop*/
                --stack_size;
            else{/*push*/
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
            temp=quot_e+1;
            delim=strchr(temp,' ');
        }
        else if(*temp == 39&&quot_flag){
            quot_s=temp;
            ++temp;
            quot_e=strchr(temp,34);
            *quot_e='\0';
            temp=quot_e+1;
            delim=strchr(temp,' ');
            quot_flag=1;
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


