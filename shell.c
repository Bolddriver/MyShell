#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>

//拆分参数，忽略双引号
int parse(char* buf, char** args)
{
    // 遍历要解析的命令的每个字符，
    // 把命令的每个参数的第一个字符的地址赋值给args数组
    // 把每个参数的最后一个字符的下一个字符修改为\0结束符
    int num = 0;
    while (*buf != '\0')
    {
        // 定位到命令中每个字符串的第一个非空的字符
        while ((*buf == ' ') || (*buf == '\t' || (*buf == '\n')))
            *buf++ = '\0';
        // 将找到的非空字符串 依次赋值给args[i]
        *args++ = buf;   
        ++num;
        // 正常的字母就往后移动，直至定位到非空字符后面的第一个空格。
        while ((*buf != '\0') && (*buf != ' ') && (*buf != '\t') && (*buf != '\n'))
            buf++;
    }
    *args = '\0';
    return num;
}
//显示提示符
void ShowPrompt() {
    //获取绝对路径，存放在path中
    char path[1024];
    getcwd(path, sizeof(path));
    //获取pwd信息
    struct passwd* pwd;
    pwd = getpwuid(getuid());
    //获得主机名
    char hostname[100] = { 0 };
    gethostname(hostname, sizeof(hostname));
    // 输出提示符
    // printf("\033[1;35mMyShell@\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m$ ", pwd->pw_name, hostname, path);
    //未着色
    printf("MyShell@%s@%s:%s$ ", pwd->pw_name, hostname, path);
    /*着色：
        \033[1;32m 粗体(1)绿色(32)
        \033[0m 清除样式
        \033[1;34m 粗体(1)蓝色(34)
        \033[1;35m 粗体(1)品红(35)
    */
}

//执行普通命令
void Exec(char* args[]) {
    //通过 fork 创建一个子进程
    pid_t  pid;
    if ((pid = fork()) < 0) //fork 
    {
        printf("fork error,please reput command\n");
        return;
    }
    else if (pid == 0)
    {
        //child
        //execvp在当前进程中执行一个新的程序
        execvp(*args, args);
        //execvp执行失败则执行下面的语句
        printf("couldn't execute: %s\n", args[0]);
        exit(127);
    }

    //parent
    int status;
    if ((pid = waitpid(pid, &status, 0)) < 0)
        printf("waitpid error\n");
}
//判断是否是管道命令
int IsPipe(char* args[])
{
    int i = 0;
    while (args[i] != NULL)
    {
        if (strcmp(args[i], "|") == 0)
        {
            return i + 1; //i是管道，则i+1就是管道的下一个命令
        }
        ++i;
    }
    return 0;
}
//从|拆分管道命令
void ParsePipe(char* input[], char* output1[], char* output2[])
{
    int i = 0;
    int size1 = 0;
    int size2 = 0;
    int ret = IsPipe(input);

    while (strcmp(input[i], "|") != 0)
    {
        output1[size1++] = input[i++];
    }
    output1[size1] = NULL;

    int j = ret;//j指向管道的后面那个字符
    while (input[j] != NULL)
    {
        output2[size2++] = input[j++];
    }

    output2[size2] = NULL;
}
//执行管道命令
void ExecvPipe(char* args1[], char* args2[])
{
    int pipefd[2]; //管道
    pid_t pid1, pid2; //子进程

    // 创建管道
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // 父进程创建子进程1
    pid1 = fork();

    if (pid1 == -1){ //创建失败
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid1 == 0){
        // 子进程1：标准输出重定向到写端，执行命令args1
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(*args1,args1);
        perror("execvp args1");
        exit(EXIT_FAILURE);
    } else {
        // 父进程创建子进程2
        pid2 = fork();
        if (pid2 == -1){ //创建失败
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid2 == 0){
            // 子进程2：标准输入重定向到管道读端，执行命令args2
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            execvp(*args2,args2);
            perror("execvp grep");
            exit(EXIT_FAILURE);
        } else {
            // 父进程关闭管道，等待子进程结束
            close(pipefd[0]);
            close(pipefd[1]);
            wait(NULL);
            wait(NULL);
        }
    }
}

int main(void)
{
    while (1)
    {
        //显示提示符
        ShowPrompt();

        // 获取用户输入
        char buf[4096];
        char* rt = fgets(buf, 4096, stdin);
        if (rt == NULL)
        {
            printf("fgets error\n");
            exit(1);
        }

        // 只输入了回车键
        if (!strcmp(buf, "\n"))
            continue;

        // 去掉换行
        if (buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = 0;

        // 解析输入，获取参数和参数的数量
        char* args[64]; //64个参数
        int argnum = parse(buf, args);

        //使用--color=always着色
        // args[argnum]="--color=always";
        // args[argnum+1]='\0';

        //两个预设的命令 exit和ver
        if (strcmp(args[0], "exit") == 0)
            exit(0);
        else if (strcmp(args[0], "ver") == 0)
            printf("mysh version 1.0, Written by ABC\n");
        //执行其他的命令
        else
        {
            if(IsPipe(args)>0){
                char* args1[32];
                char* args2[32];
                ParsePipe(args,args1,args2);
                ExecvPipe(args1,args2);
            }
            else Exec(args);
        }
    }

    exit(0);
    return 0;
}


