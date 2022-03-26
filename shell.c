#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "regex.h"
#include <fcntl.h>

#define CMD_LENGTH 256
#define CMD_NUMBER 5
#define ARGS_NUM 10
#define PATH_LENGTH 500
#define MAX_BUFFER 300

typedef struct _command{
    int begin, end;
    int argsNum;
    int to;
    char toFile[PATH_LENGTH]; 
    char* args[ARGS_NUM];
    struct _command* next;
} command;

command cmdArray[CMD_NUMBER];
int cmdNum;

void initCmd(command *cmdPtr);

// read input from user
char* readInput(){
    int cmdLength = CMD_LENGTH;
    // allocate memroy for command
    char* command = malloc(sizeof(char*) * cmdLength);
    if(!command){
        fprintf(stderr, "Allocation error!\n");
        exit(EXIT_FAILURE);
    }

    int character;
    int pos = 0;

    while(1){
        character = getchar();
        // get the end of the stdin
        if(character == EOF){
            exit(EXIT_SUCCESS);
            // get the end of the input
        }else if(character == '\n'){
            command[pos] = '\0';
            return command;
            // assign the input to the command string
        }else{
            command[pos] = character;
        }
        pos++;

        // reallocate command string if it is not enough
        if(pos >= cmdLength){
            cmdLength += CMD_LENGTH;
            command = realloc(command, cmdLength);
            if(!command){
                fprintf(stderr, "Allocation error!\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

// parse the input
void parseInput(char* command){
    int length = strlen(command);
    int i = 0;
    int begin = 0;
    for(i = 0; i <= length; i ++){
        switch(command[i]){
            case '\t':
                command[i] = ' ';
                break;
            case ' ':
                break;
            case '\0':
                cmdArray[cmdNum].end = i;
                cmdNum ++;
                break;
            default:
                if(!begin){
                    begin = 1;
                    cmdArray[cmdNum].begin = i;
                }
        }
    }
}

// parse args
int parseArgs(char* cmdStr){
    int begin, end, character;
    int beginPath = 0, beginSingleQuote = 0, beginDoubleQuote = 0;
    int i = 0;
    command *cmdPtr;
    for(i = 0; i < cmdNum; i ++){
        cmdPtr = &cmdArray[i];
        begin = cmdPtr->begin;
        end = cmdPtr->end;
        initCmd(cmdPtr);
        int j = begin;
        for(j = begin; j < end; j ++){
            character = cmdStr[j];
            // redirect or pipline
            if(character == '>' || character == '|'){
                if(beginPath){
                    beginPath = 0;
                }
                cmdStr[j] = '\0';
            }
            // right redirect
            if(character == '>'){
                if(cmdStr[j + 1] == '>'){
                    cmdPtr->to += 2;
                }
                int pos = getPath(cmdPtr->toFile, cmdStr, j);
                if(pos > 0){
                    j = pos;
                }
                // pipe
            }else if(character == '|'){
                cmdPtr->end = i;
                // create new command node
                cmdPtr->next = (command*)malloc(sizeof(command));
                cmdPtr = cmdPtr->next;
                initCmd(cmdPtr);
            }else if(character == ' ' || character == '\0'){
                if(beginPath){
                    beginPath = 0;
                    cmdStr[j] = '\0';
                }
            }else if(character == '\''){
                if(!beginSingleQuote){
                    beginSingleQuote = 1;
                }else{
                    cmdStr[j] = '\0';
                }
                continue;
            }else if(character == '\"'){
                if(!beginDoubleQuote){
                    beginDoubleQuote = 1;
                }else{
                    cmdStr[j] = '\0';
                }
                continue;
            }else{
                if(cmdPtr->begin == -1){
                    cmdPtr->begin = i;
                }
                if(!beginPath){
                    beginPath = 1;
                    cmdPtr->args[cmdPtr->argsNum] = cmdStr + j; 
                    cmdPtr->argsNum++;
                }
            }     
        }     
        cmdPtr->end=end;
    }
}

// initilize the command structure
void initCmd(command *cmdPtr){
    cmdPtr->argsNum = 0;
    int i = 0;
    cmdPtr->to = 0;
    for(i = 0; i < ARGS_NUM; i ++){
        cmdPtr->args[i] = NULL;
    }
    cmdPtr->next = NULL;
}

// get the file path from command string
int getPath(char* file, char* command, int pos){
    while(command[pos] != ' '){
        pos++;
    }
    pos ++;
    // no file
    if(command[pos] == '\n'){
        return -1;
    }
    int i = 0;
    char character;

    while(character = file[i] = command[pos]){
        if(character == ' ' || character == '|' || character == '<' || character == '>' || character == '\n'){
            break;
        }
        ++i;
        ++pos;
    }
    file[i] = '\0';
    return pos - 1;
}

// built in commands
int builtin(command* cmdPtr){
    if(!cmdPtr->args[0]){
        return 0;
    }
    
    if(!strcmp(cmdPtr->args[0], "info")){
        printf("\033[34mCOMP2211 Simplified Shell by sc192yl\n\033[30m");
        return 0;
    }
    if(!strcmp(cmdPtr->args[0], "exit")){
        printf("\033[34mThanks for using this shell\n\033[30m");
        exit(0);
    }
    if(!strcmp(cmdPtr->args[0], "pwd")){
        printf("\033[34m%s\n\033[30m", getcwd(cmdPtr->args[1], PATH_LENGTH));
        return 0;
    }

    if(!strcmp(cmdPtr->args[0], "cd")){
        struct stat file;
        if(cmdPtr->args[1]){
            stat(cmdPtr->args[1], &file);
            if(S_ISDIR(file.st_mode)){
                chdir(cmdPtr->args[1]);
                printf("\033[34mChange Successfully!\n\033[30m");
            }else{
                printf("\033[34mcd: '%s': No such directory\n\033[30m", cmdPtr->args[1]);
                return 0;
            }
        }
        return 0;
    }

    if(!strcmp(cmdPtr->args[0], "mygrep")){
        if(cmdPtr->argsNum < 3){
            fprintf(stderr, "Lack of parameters\n");
            exit(-1);
        }
        char* pattern = cmdPtr->args[1];
        char* file = cmdPtr->args[2];
        if(strlen(file) == 0){
            fprintf(stderr, "No input file!\n");
            exit(-1);
        }
        FILE* fp = fopen(file, "r");
        if(!fp){
            fprintf(stderr, "Cannot open file!\n");
            exit(-1);
        }
        int cnt = 0;
        
        char errorBuffer[200];
        regmatch_t rm[100];
        regex_t reg;
        int ret;
        char buffer[MAX_BUFFER];

        // compile regular expression string
        ret = regcomp(&reg, pattern, REG_EXTENDED);
        if(ret){
            regerror(ret, &reg, errorBuffer, 200);
            fprintf(stderr, "%s\n", errorBuffer);
            memset(errorBuffer, '\0', 200);
            exit(-1);
        }

        while(fgets(buffer, sizeof(buffer), fp) != 0){
            // match regular expression text
            ret = regexec(&reg, buffer, 100, rm, 0);
            if(ret){
                memset(buffer, '\0', MAX_BUFFER);
                continue;
            }else{
                // print the matching text;
                printf("%s", buffer);
                cnt ++;
            }
            memset(buffer, '\0', MAX_BUFFER);
        }
        if(!cnt){
            printf("No match!\n");
        }
        return 0;
    }
    return 1;
}

void multiPro(command* cmdPtr, int in, int out){
    // redirect
    if(cmdPtr->to){
        int out = open(cmdPtr->toFile, O_CREAT|O_WRONLY|O_APPEND, 0600);
        dup2(out, STDOUT_FILENO);
        close(out);
    }
    // pipeline
    // in program
    if(in != STDIN_FILENO){
        dup2(in, STDIN_FILENO);
        close(in);
    }
    // out program
    if(out != STDOUT_FILENO){
        dup2(out, STDOUT_FILENO);
        close(out);
    }
}

void execution(command* cmdPtr){
    char *arguments[ARGS_NUM - 1];
    int i = 0;
    for(i = 0; i < ARGS_NUM - 1; i ++){
        arguments[i] = cmdPtr->args[i + 1];
    }
    execvp(cmdPtr->args[1], arguments);
}

int launchOut(command* cmdPtr){
    if(!cmdPtr->next){
        multiPro(cmdPtr, STDIN_FILENO,STDOUT_FILENO);
        if(!strcmp(cmdPtr->args[0], "ex")){
            execution(cmdPtr);
        }
    }

    // create pipe
    int fd[2];
    pipe(fd);
    pid_t pid = fork();
    if(pid < 0){
        // forking error
        perror("Forking error!\n");
    }else if(!pid){
        // child process
        // close input 
        close(fd[0]);
        multiPro(cmdPtr, STDIN_FILENO, fd[1]);
        if(!strcmp(cmdPtr->args[0], "ex")){
            execution(cmdPtr);
        }
    }else{
        // parent process        
        // wait for the main process
        wait(NULL);
        cmdPtr = cmdPtr->next;
        //close output
        close(fd[1]);
        multiPro(cmdPtr, fd[0], STDOUT_FILENO);
        if(!strcmp(cmdPtr->args[0], "ex")){
            execution(cmdPtr);
        }
    }
}

// for debug purpose
void debug(command* pcmd){
    command*p;
    for(p = pcmd; p; p=p->next){
        printf("****************\n");
        int i;
        for(i=0; i<p->argsNum; ++i){
            printf("args[%d]:%s\n",i+1,p->args[i]);
        }
    }
}

int shellLoop(){
    while(1){
        cmdNum = 0;
        printf("\033[1;34mmyshell>>\033[0;30m ");
        fflush(stdin);
        char* character;
        character = readInput();
        if(!character){
            continue;
        }
        parseInput(character);
        parseArgs(character);
        int i = 0;
        for(i = 0; i < cmdNum; i ++){
            command *cmdPtr = cmdArray + i;
            command *tmp;
            int status = builtin(cmdPtr);
            if(status){
                pid_t pid = fork();
                if(!pid){
                    // child process
                    launchOut(cmdPtr);
                }else if(pid < 0){
                    // forking error
                    perror("Forking error!\n");
                }else{
                    wait(NULL);
                }
            }
            // free pointer
            cmdPtr = cmdPtr->next;
            while(cmdPtr){
                tmp = cmdPtr->next;
                free(cmdPtr);
                cmdPtr = tmp;
            }
        }
    }
}

int main(){   
    shellLoop();
    return 0;
}