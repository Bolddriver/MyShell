#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <readline/readline.h>
#include <readline/history.h> //用于实现命令列表和命令补全

#define MAX_ALIAS 100 //最多能设置的别名数量
#define LEN_ALIAS 20 //别名的长度
int AliasCount = 0; //已经设置的别名数量
struct Aliases {
    char* Key;
    char* Value;
} aliases[MAX_ALIAS];

// 添加别名
void addAlias(char* key, char* value) {
    if (AliasCount < MAX_ALIAS) {
        aliases[AliasCount].Key = (char*)malloc(strlen(key) + 1);
        aliases[AliasCount].Value = (char*)malloc(strlen(value) + 1);

        if (aliases[AliasCount].Key && aliases[AliasCount].Value) {
            strcpy(aliases[AliasCount].Key, key);
            strcpy(aliases[AliasCount].Value, value);
            AliasCount++;
        }
        else {
            printf("内存分配失败，无法添加别名。\n");
        }
    }
    else {
        printf("别名存储空间已满，无法添加。\n");
    }
}

// 根据键查找别名
char* findAlias(char* key) {
    for (int i = 0; i < AliasCount; i++) {
        if (strcmp(aliases[i].Key, key) == 0) {
            return aliases[i].Value;
        }
    }
    return NULL; // 如果未找到返回空指针
}

// 根据键删除别名
void deleteAlias(char* key) {
    for (int i = 0; i < AliasCount; i++) {
        if (strcmp(aliases[i].Key, key) == 0) {
            free(aliases[i].Key);
            free(aliases[i].Value);

            // 将最后一个别名移到当前位置，然后减少别名数量
            aliases[i] = aliases[AliasCount - 1];
            AliasCount--;
            return;
        }
    }
    printf("未找到别名，无法删除。\n");
}

// 更新别名的值
void updateAlias(char* key, char* newValue) {
    for (int i = 0; i < AliasCount; i++) {
        if (strcmp(aliases[i].Key, key) == 0) {
            free(aliases[i].Value);
            aliases[i].Value = (char*)malloc(strlen(newValue) + 1);

            if (aliases[i].Value) {
                strcpy(aliases[i].Value, newValue);
            }
            else {
                printf("内存分配失败，无法更新别名。\n");
            }
            return;
        }
    }
    printf("未找到别名，无法更新。\n");
}

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
        while ((*buf != '\0') && (*buf != ' ') && (*buf != '\t') && (*buf != '\n')) {
            // 确保正确识别被双引号包裹的参数
            if (*buf == '"') {
                buf++;
                while (*buf != '"')
                    buf++;
            }
            buf++;
        }

    }
    *args = '\0';
    return num;
}

//执行普通命令
void Exec(char* args[]) {
    //通过 fork 创建一个子进程
    pid_t  pid;
    if ((pid = fork()) < 0)
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

    if (pid1 == -1) { //创建失败
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid1 == 0) {
        // 子进程1：标准输出重定向到写端，执行命令args1
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(*args1, args1);
        perror("execvp args1");
        exit(EXIT_FAILURE);
    }
    else {
        // 父进程创建子进程2
        pid2 = fork();
        if (pid2 == -1) { //创建失败
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid2 == 0) {
            // 子进程2：标准输入重定向到管道读端，执行命令args2
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            execvp(*args2, args2);
            perror("execvp grep");
            exit(EXIT_FAILURE);
        }
        else {
            // 父进程关闭管道，等待子进程结束
            close(pipefd[0]);
            close(pipefd[1]);
            wait(NULL);
            wait(NULL);
        }
    }
}

// 拆分等式
int ParseEquality(char* input, char* key, char* value) {
    char* ptr = strchr(input, '=');
    if (ptr != NULL) {
        size_t keyLength = ptr - input; // 计算键的长度
        strncpy(key, input, keyLength);
        key[keyLength] = '\0'; // 手动添加字符串结束符
        strcpy(value, ptr + 1);
        printf("Key: %s, Value: %s\n", key, value);
        return 0;
    }
    else return 1;
}

// 别名
void Alias(char* args[]) {
    // 1.拆分命令和参数（已经拆分好）
    // 2.检查有无参数
    if (args[1] == NULL) {
        // 无参数，输出所有别名
        for (int i = 0; i < AliasCount; i++) {
            printf("%s=%s\n", aliases[i].Key, aliases[i].Value);
        }
    }
    else {
        // 有参数，依次处理每一个参数
        for (int i = 1;args[i] != NULL;i++) {
            //检查参数中有无等号
            char AKey[LEN_ALIAS]; //键
            char AValue[LEN_ALIAS]; //值
            //有等号，添加别名
            if (ParseEquality(args[i], AKey, AValue) == 0) {
                if (findAlias(AKey) == NULL)
                    addAlias(AKey, AValue);
                else updateAlias(AKey, AValue);
            }
            //没有等号，查询别名
            else {
                char* result = findAlias(args[i]);
                if (result != NULL) {
                    printf("%s=%s\n", args[i], result);
                }
            }
        }
    }
}

//取消别名
void UnAlias(char* args[]) {
    if (args[1] == NULL) {
        // 无参数，输出用法
        printf("unalias: usage: unalias [-a] name [name ...]\n");
    }
    else {
        // 有参数，依次处理每一个参数
        for (int i = 1;args[i] != NULL;i++) {
            deleteAlias(args[i]);
        }
    }
}

void LoadAlias() {
    FILE* fp = fopen("./alias.txt", "a+");
    char line1[LEN_ALIAS]; // 键
    char line2[LEN_ALIAS]; // 值

    while (fgets(line1, sizeof(line1), fp) != NULL && fgets(line2, sizeof(line2), fp) != NULL) {
        line1[strlen(line1) - 1] = '\0';
        line2[strlen(line2) - 1] = '\0';
        if (findAlias(line1) == NULL)
            addAlias(line1, line2);
        else updateAlias(line1, line2);
    }
    fclose(fp);
}

void SaveAlias() {
    FILE* fp = fopen("./alias.txt", "a");
    for (int i = 0; i < AliasCount; i++) {
        fprintf(fp, "%s\n%s\n", aliases[i].Key, aliases[i].Value);
    }
    fclose(fp);
}

int main(void)
{
    LoadAlias();
    //按下TAB自动补全
    rl_bind_key('\t', rl_complete);
    while (1)
    {
        //显示提示符
        char hostname[100] = { 0 };
        gethostname(hostname, sizeof(hostname));

        char shell_prompt[200];
        snprintf(shell_prompt, sizeof(shell_prompt), "MyShell@%s@%s:%s$ ", getenv("USER"), hostname, getcwd(NULL, 1024));
        // snprintf(shell_prompt, sizeof(shell_prompt), "\033[1;35mMyShell\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m$ ", getenv("USER"), hostname, getcwd(NULL,1024));

        // 获取用户输入
        char* buf;
        buf = readline(shell_prompt);
        add_history(buf);

        // 解析输入，获取参数和参数的数量
        char* args[64]; //64个命令和参数
        int argnum = parse(buf, args);

        //使用--color=always着色
        // args[argnum]="--color=always";
        // args[argnum+1]='\0';

        if (strcmp(args[0], "exit") == 0)
            break;
        else if (strcmp(args[0], "ver") == 0)
            printf("mysh version 1.0, Written by ABC\n");
        else if (strcmp(args[0], "alias") == 0)
            Alias(args);
        else if (strcmp(args[0], "unalias") == 0)
            UnAlias(args);
        else
        {
            // 执行前把别名替换成原名
            if (IsPipe(args) > 0) {
                char* args1[32];
                char* args2[32];
                ParsePipe(args, args1, args2);
                if (findAlias(args1[0]) != NULL) {
                    strcpy(args1[0], findAlias(args1[0]));
                }
                // printf("%s %s\n",args1[0],args2[0]);
                ExecvPipe(args1, args2);
            }
            else {
                if (findAlias(args[0]) != NULL) {
                    strcpy(args[0], findAlias(args[0]));
                }
                Exec(args);
            }
        }
        free(buf);
    }


    SaveAlias();

    exit(0);
    return 0;
}


