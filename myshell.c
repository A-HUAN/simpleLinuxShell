#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>

#define     MAXSIZE 256
#define     COLOR_NONE          "\033[0m"
#define     FONT_COLOR_RED      "\033[0;31m"
#define     FONT_COLOR_BLUE     "\033[1;34m"
#define     FONT_COLOR_YELLOW   "\033[1;33m"

char userName[MAXSIZE]; //用户名
char hostName[MAXSIZE]; //主机名
char curPath[MAXSIZE];//存放当前工作路径
char parseCommand[10][MAXSIZE];//存放解析后的命令

/*获取当前用户名函数*/
void getUserName(){
    struct passwd* user = getpwuid(getuid());
    strcpy(userName,user->pw_name);
}
/*获取当前主机名函数*/
void getHostName(){
    gethostname(hostName, sizeof(hostName));
}
/*获取当前工作路径函数 失败返回0 成功返回1*/
void getCurWorkdir(){
    char *cwd=getcwd(curPath,MAXSIZE);
    if(cwd==NULL) {
        perror("Error:getCurWorkdir!\n");
        exit(-1);
    }
    else
        return;
}
/*命令解析函数*/
int parse(char *command){
    int num=0;
    int i,j;
    int len=(int)strlen(command);
    for(i=0,j=0;i<len;i++) {
        if (command[i] != ' ')
            parseCommand[num][j++] = command[i];
        else {
            if (j != 0) {
                parseCommand[num++][j] = '\0';
                j = 0;
            }
        }
    }
    if(j!=0){
        parseCommand[num++][j]='\0';
    }
    return num;
}
/* cd命令函数 */
void excute_cd(int command_num){
    if(command_num!=2) {
        perror("Error:command num!\n"); //命令参数不为2
        exit(-1);
    }
    int ret=chdir(parseCommand[1]);
    if(ret) {
        perror(("Error:no such path!\n")); //切换路径失败
        exit(-1);
    }
    else
        return ;
}
/* history命令 打印函数 */
void printf_history(){
    FILE *fp;
    int cmdNUm=1;
    if((fp=fopen("history.txt","r"))<0)
        perror("Error:No history file\n");
    char history[MAXSIZE];
    while(fgets(history, sizeof(history),fp)){
        printf("%d  %s", cmdNUm++,history);
    }
    printf("\n");
    fclose(fp);
}
/* 从History文件获取命令*/
char* getHisCom(int line){
    int flag=0;
    char command[MAXSIZE];
    char *getCommand=NULL;
    getCommand=(char *)malloc(MAXSIZE);
    FILE *fp;
    if((fp=fopen("history.txt","r"))<0)
        perror("Error:history file open\n");
    while(fgets(command, sizeof(command),fp)){
        line--;
        if(line==0){
            strcpy(getCommand,command);
            flag=1;
            break;
        }
    }
    fclose(fp);
    if(flag)
        return getCommand;
    else {
        printf("没有此条记录\n");
        return NULL;
    }
}

/* 记录用户输入命令存入history文件 */
void write_history(char *command){
    FILE *fp;
    if((fp=fopen("history.txt","a+"))<0)
        perror("Error:History file open\n");
    if(strcmp(command,"\n")==0) {
        return;
    }
    if(command[0]=='!' && atoi(command+1)>0){
        return;
    }
    fseek(fp,0,SEEK_END);
    if( ftell(fp) != 0 ){
        fputc('\n',fp);
    }
    fputs(command,fp);
}

/* 指令最终执行函数 */
void processExec(int l,int r){
    char *inFile=NULL; //输入标识符
    char *outFile=NULL;//输出标识符
    int inFlag=0; //‘<’个数
    int outFlag=0;//‘>’个数
    int endIndex=r; //重定向标志下标
    for(int i=l;i<r;i++){
        if(strcmp(parseCommand[i],"<")==0) {
            inFile = parseCommand[i + 1]; //输入重定向为'<'后面的指令
            inFlag=1;
            endIndex=i;
        }
        else if(strcmp(parseCommand[i],">")==0) {
            outFile = parseCommand[i + 1]; //输出重定向为'>'后面的指令
            outFlag=1;
            endIndex=i;
        }
        else if(strcmp(parseCommand[i],">>")==0) {
            outFile = parseCommand[i + 1]; //输出重定向为'>>'后面的指令
            outFlag=2;
            endIndex=i;
        }
    }
    pid_t pid=vfork();
    if(pid<0){
        perror("Error:fork\n");
        exit(-1);
    }
    else if(pid==0) {
        /* 根据重定向标志变量执行对应操作 */
        if (inFlag == 1) {
            int fd = open(inFile, O_RDONLY);
            dup2(fd, STDIN_FILENO);
            //freopen(inFile, "r", stdin);
        }
        if (outFlag == 1) {
            int fd;
            fd = open(outFile, (unsigned)O_WRONLY | (unsigned)O_CREAT, 0666);
            dup2(fd,STDOUT_FILENO);
            //freopen(outFile, "w", stdout);
        }
        if (outFlag == 2) {
            int fd;
            fd = open(outFile, (unsigned)O_RDWR | (unsigned)O_APPEND | (unsigned)O_CREAT, 0666);
            dup2(fd,STDOUT_FILENO);
            //freopen(outFile, "a", stdout);
        }
        /* 获取重定向标志之前的指令 */
        char *tmpCom[MAXSIZE];
        for (int i = l; i < endIndex; i++) {
            tmpCom[i] = parseCommand[i];
        }
        tmpCom[endIndex] = NULL;
        /* 执行重定向之前指令 */
        execvp(tmpCom[l], tmpCom + l);
        exit(0);
    }
    else {
        int status;
        waitpid(pid, &status, 0);
        return ;
    }
}
/* 判断是否有‘|’函数 */
void processWithPipe(int l,int r){
    if(l>=r)
        return ;
    int pipeIndex=-1; //‘|’下标初始化
    for(int i=l;i<r;i++){
        if(strcmp(parseCommand[i],"|")==0) {
            pipeIndex = i;
            break;
        }
    }
    /* 没有‘|’ 跳转执行 */
    if(pipeIndex==-1) {
        processExec(l, r);
        return ;
    }
    /* 含有‘|’ */
    /* 创建进程通信管道 */
    int fds[2];
    if (pipe(fds) == -1) {
        perror("Error:pipe\n");
        exit(-1);
    }

    pid_t pid = vfork();
    if (pid <0 ) {
        perror("Error:fork!\n");
        exit(-1);
    }
    /* 子进程执行单个命令 */
    else if (pid == 0) {
        /* 关闭读端 标准输出重定向为管道写端 */
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO); // 将标准输出重定向到fds[1]
        /* 执行‘|’之前指令 */
        processExec(l,pipeIndex);
        close(fds[1]);
        exit(0);
    }
    /* 父进程递归执行后续命令 */
    else {
        int status;
        waitpid(pid, &status, 0);
        if (pipeIndex+1 < r){
            /* 关闭写端 标准输入重定向为管道读端 */
            close(fds[1]);
            dup2(fds[0], STDIN_FILENO); // 将标准输入重定向到fds[0]
            processWithPipe(pipeIndex+1, r); // 递归执行后续指令
            close(fds[0]);
        }
        return ;
    }
}
/* 命令 */
void process(int command_num){
    /* 创建子进程 */
    pid_t pid=vfork();
    if(pid<0){
        perror("Error:fork\n");
        exit(-1);
    }
    /* 子进程执行对应命令 */
    else if(pid==0){
        /* 进入管道处理函数 */
        processWithPipe(0,command_num);
        exit(0);
    }
    /* 父进程回收子进程 */
    else {
        int status;
        waitpid(pid, &status, 0);
        return ;
    }
}

int main(){
    getUserName(); //获取用户名
    getHostName(); //获取主机名
    /*获取当前工作路径 失败则输出提示并退出*/
    getCurWorkdir();
    char input_command[MAXSIZE];//存储用户输入命令
    while(1){
        /*打印当前工作路径*/
        printf(FONT_COLOR_BLUE"%s""@"FONT_COLOR_YELLOW"%s"":"FONT_COLOR_RED"%s""$ "COLOR_NONE
                ,userName,hostName,curPath);
        /*获取用户输入命令 并将末尾改为\0*/
        fgets(input_command, sizeof(input_command),stdin);
        input_command[strlen(input_command)-1]='\0';
        write_history(input_command);
        int command_num=parse(input_command);
        if(command_num!=0) {
            /*exit命令处理*/
            if (strcmp(parseCommand[0], "exit") == 0)
                exit(0);
            /*cd命令处理*/
            else if (strcmp(parseCommand[0], "cd") == 0) {
                excute_cd(command_num);
                getCurWorkdir();
            }
            else if(strcmp(parseCommand[0], "history") == 0){
                printf_history();
            }
            else if(input_command[0]=='!' && atoi(input_command+1)>0){
                strcpy(input_command,getHisCom(atoi(input_command+1)));
                printf("%s",input_command);
                char *find=strchr(input_command,'\n');
                if(find)
                    *find='\0';
                //input_command[strlen(input_command)]='\0';
                for(int i=0;i<strlen(input_command);i++)
                    printf("%d ",*(input_command+i));
                command_num=parse(input_command);
                write_history(input_command);
                process(command_num);
            }
            /*其他命令处理*/
            else{
                process(command_num);
            }
        }
    }
}
