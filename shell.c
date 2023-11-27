#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>

//拆分参数
int parse(char* buf, char** args)
{
    int num = 0;
    while (*buf != '\0')
    {
        while ((*buf == ' ') || (*buf == '\t' || (*buf == '\n')))
            *buf++ = '\0'; //该循环是定位到命令中每个字符串的第一个非空的字符
        *args++ = buf;   //将找到的非空字符串 依次赋值给args［i］。
        ++num;

        while ((*buf != '\0') && (*buf != ' ') && (*buf != '\t') && (*buf != '\n'))//正常的字母就往后移动，直至定位到非空字符后面的第一个空格。
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
    printf("-\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m$ ", pwd->pw_name, hostname, path);
    /*文本着色：
        \033[1;32m 粗体(1)绿色(32)
        \033[0m 清除样式
        \033[1;34m 粗体(1)蓝色(34)
    */
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
        {
            //printf("%% ");      // print prompt
            continue;           // end loop this time
        }

        // replace newline with null
        if (buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = 0;

        // 分析输入，获取参数和参数的数量
        char* args[64];
        int argnum = parse(buf, args);

        //两个预设的命令 exit和ver
        if (strcmp(args[0], "exit") == 0)
            exit(0);
        else if (strcmp(args[0], "ver") == 0)
            printf("mysh version 1.0, Written by ABC\n");
        //执行其他的命令
        else
        {
            //通过 fork 创建一个子进程
            //在子进程中使用 execvp 来执行用户输入的命令
            //如果命令无法执行，打印出错误信息
            pid_t  pid;
            if ((pid = fork()) < 0) //fork 
            {
                printf("fork error,please reput command\n");
                continue;       //end loop this time
            }
            else if (pid == 0)
            {
                //child
                execvp(*args, args);
                printf("couldn't execute: %s\n", buf);
                exit(127);
            }

            //parent
            int status;
            if ((pid = waitpid(pid, &status, 0)) < 0)
                printf("waitpid error\n");
        }
    }

    exit(0);
    return 0;
}


