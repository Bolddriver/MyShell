#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <readline/readline.h>
#include <readline/history.h> //用于实现命令列表和命令补全
#include <fcntl.h>

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
    // 对于不包含引号的命令：
    // 遍历要解析的命令的每个字符，
    // 把命令的每个参数的第一个字符的地址赋值给args数组
    // 把参数之间的分隔符修改为\0结束符
    // args是指向buf的指针。在buf中，把每一个空格、制表符、换行符改成了\0，把buf分割成多个字符串
    int num = 0;
    while (buf!=NULL && *buf != '\0')
    {
        // 定位到命令中每个字符串的第一个非空的字符
        while ((*buf == ' ') || (*buf == '\t' || (*buf == '\n')))
            *buf++ = '\0';

        // args[i]赋值为当前状态下buf的地址
        *args = buf;
        // 正常的字母就往后移动，直至定位到非空字符后面的第一个空格。
        while ((*buf != '\0') && (*buf != ' ') && (*buf != '\t') && (*buf != '\n')) {
            // 确保正确识别被双引号包裹的参数
            if (*buf == '"') { //读取到第一个双引号时
                buf++; //跳过这个双引号
                *args=buf; //args[i]的地址改成双引号后的字符的地址
                while (*buf != '"') //后移，直到读取到第二个双引号
                    buf++;
                *buf = '\0'; //在buf中删掉第二个双引号
            }
            buf++;
        }
        *args++; //args后移，准备读取下一条命令
        num++; //命令计数器增加
    }
    *args = '\0';
    return num;
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

void nExecPipe(char* args[]);

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
        // snprintf(shell_prompt, sizeof(shell_prompt), "MyShell@%s@%s:%s$ ", getenv("USER"), hostname, getcwd(NULL, 1024));
        snprintf(shell_prompt, sizeof(shell_prompt), "\033[1;35mMyShell\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m$ ", getenv("USER"), hostname, getcwd(NULL,1024));

        // 获取用户输入
        char* buf;
        buf = readline(shell_prompt);
        while(buf==NULL || strcmp(buf,"")==0){
            buf = readline(shell_prompt);
        }
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
            printf("MyShell version 1.0, written by Bolddriver. Made by heart.\n");
        else if (strcmp(args[0], "alias") == 0)
            Alias(args);
        else if (strcmp(args[0], "unalias") == 0)
            UnAlias(args);
        else if (strcmp(args[0], "cd") == 0) //更改工作目录
            chdir(args[1]);
        else
        {
            // 执行前把别名替换成原名
            if (findAlias(args[0]) != NULL) {
                strcpy(args[0], findAlias(args[0]));
            }
            nExecPipe(args);
            
        }
        free(buf);
    }
    SaveAlias();
    exit(0);
    return 0;
}



void nExecPipe(char* args[]) {
    // 保存当前的文件描述符
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stdin = dup(STDIN_FILENO);

    int npid = 1;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            npid++;
        }
    }

    int iargs = 0;
    int pipefds[npid - 1][2]; // 为每个管道创建文件描述符

    for (int i = 0; i < npid; i++) {
        char* carg[32]; //当前进程要执行的命令和参数
        int icarg = 0;
        // 拆分命令
        while (args[iargs] != NULL && strcmp(args[iargs], "|") != 0) {
            if (strcmp(args[iargs], "<") == 0) {
                // 输入重定向
                iargs++;
                int fd = open(args[iargs], O_RDONLY);
                dup2(fd, STDIN_FILENO);
                close(fd);
            } else if (strcmp(args[iargs], ">") == 0) {
                // 输出重定向（覆盖写）
                iargs++;
                int fd = open(args[iargs], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            } else if (strcmp(args[iargs], ">>") == 0) {
                // 输出重定向（追加写）
                iargs++;
                int fd = open(args[iargs], O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            } else {
                carg[icarg] = args[iargs];
                icarg++;
            }
            iargs++;
        }
        carg[icarg] = NULL;

        if (i < npid - 1) {
            if (pipe(pipefds[i]) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        pid_t pid = fork();
        if (pid == -1) { //创建失败
            printf("errorfork\n");
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { //在子进程中
            if (i > 0) { // 不是第一个命令
                dup2(pipefds[i - 1][0], STDIN_FILENO);
                close(pipefds[i - 1][0]);
                close(pipefds[i - 1][1]);
            }
            if (i < npid - 1) { // 不是最后一个命令
                close(pipefds[i][0]);
                dup2(pipefds[i][1], STDOUT_FILENO);
                close(pipefds[i][1]);
            }

            execvp(carg[0], carg);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else { //父进程
            if (i > 0) { // 不是第一个命令
                close(pipefds[i - 1][0]);
                close(pipefds[i - 1][1]);
            }
            wait(NULL);

            // 恢复文件描述符
            fflush(stdout);
            fflush(stdin);
            dup2(saved_stdout, STDOUT_FILENO);
            dup2(saved_stdin, STDIN_FILENO);
            fflush(stdout);
            fflush(stdin);
        }
        
        if (args[iargs] != NULL) {
            iargs++; // 跳过管道符号
        }
    }
}

