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

// 定义ANSI转义序列，用于为提示符着色
#define ANSI_COLOR_RESET   "\001\033[0m\002" //重置样式
#define ANSI_COLOR_MAGENTA_BOLD "\001\033[1;35m\002" //品红色粗体
#define ANSI_COLOR_GREEN_BOLD  "\001\033[1;32m\002" //绿色粗体
#define ANSI_COLOR_BLUE_BOLD "\001\033[1;34m\002" //蓝色粗体

int AliasCount = 0; //已经设置的别名数量
int AliasCapacity = 2; //别名的容量
struct Aliases {
    char* Key;
    char* Value;
} *aliases;

// 增加别名的内存空间
int allocateAlias() {
    struct Aliases* temp = (struct Aliases*)realloc(aliases, (AliasCapacity + 2) * sizeof(struct Aliases));
    if (temp == NULL) {
        free(aliases);
        return -1;
    }
    else {
        AliasCapacity += 2;
        aliases = temp;
        return 0;
    }
}

// 添加别名
void addAlias(char* key, char* value) {
    if (AliasCount >= AliasCapacity) {
        if (allocateAlias() == -1) {
            printf("Failed to add '%s': can't allocate memory.\n", key);
            return;
        }
    }
    aliases[AliasCount].Key = (char*)malloc(strlen(key) + 1);
    aliases[AliasCount].Value = (char*)malloc(strlen(value) + 1);
    if (aliases[AliasCount].Key && aliases[AliasCount].Value) {
        strcpy(aliases[AliasCount].Key, key);
        strcpy(aliases[AliasCount].Value, value);
        AliasCount++;
        //printf("Add alias: %s\n",key);
    }
}

// 根据键查找别名, 返回下标值
int findAlias(char* key) {
    for (int i = 0; i < AliasCount; i++) {
        if (strcmp(aliases[i].Key, key) == 0) {
            return i;
        }
    }
    return -1; // 如果未找到返回-1
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
    printf("Failed to delete '%s': alias not found.\n", key);
}

// 更新别名的值
void updateAlias(char* key, char* newValue) {
    for (int i = 0; i < AliasCount; i++) {
        if (strcmp(aliases[i].Key, key) == 0) {
            free(aliases[i].Value);
            aliases[i].Value = (char*)malloc(strlen(newValue) + 1);

            if (aliases[i].Value) {
                strcpy(aliases[i].Value, newValue);
                //printf("Update alias: '%s'\n",key);
            }
            return;
        }
    }
    printf("Failed to update '%s': alias not found.\n", key);
}

//拆分参数，
//RmQuotes为1移除双引号；RmSpace为1将空格替换成\0
int parse(char* buf, char** args, int RmQuotes, int RmSpace)
{
    // 对于不包含引号的命令：
    // 遍历要解析的命令的每个字符，
    // 把命令的每个参数的第一个字符的地址赋值给args数组
    // 把参数之间的分隔符修改为\0结束符
    // args是指向buf的指针。在buf中，把每一个空格、制表符、换行符改成了\0，把buf分割成多个字符串
    int num = 0;
    while (buf != NULL && *buf != '\0')
    {
        // 定位到命令中每个字符串的第一个非空的字符
        while ((*buf == ' ') || (*buf == '\t' || (*buf == '\n'))) {
            if (RmSpace == 1) *buf = '\0';
            *buf++;
        }

        // args[i]赋值为当前状态下buf的地址
        *args = buf;
        // 正常的字母就往后移动，直至定位到非空字符后面的第一个空格。
        while ((*buf != '\0') && (*buf != ' ') && (*buf != '\t') && (*buf != '\n')) {
            // 确保正确识别被双引号包裹的参数
            if (*buf == '"') { //读取到第一个双引号时
                buf++; //跳过这个双引号
                if (RmQuotes == 1)
                    *args = buf; //args[i]的地址改成双引号后的字符的地址
                while (*buf != '"') //后移，直到读取到第二个双引号
                    buf++;
                if (RmQuotes == 1)
                    *buf = '\0'; //在buf中删掉第二个双引号
            }
            else if (*buf == '\'') { //读取到单引号时
                buf++; //跳过这个单引号
                while (*buf != '\'') //后移，直到读取到第二个双引号，避免从两个单引号中间的空格截断
                    buf++;
            }
            buf++;
        }
        *args++; //args后移，准备读取下一条命令
        num++; //命令计数器增加
    }
    if (RmSpace == 1) *args = '\0';
    return num;
}

// 拆分等式：在添加别名时使用
int ParseEquality(char* input, char* key, char* value) {
    char* ptr = strchr(input, '=');
    if (ptr != NULL) {
        size_t keyLength = ptr - input; // 计算键的长度
        strncpy(key, input, keyLength);
        key[keyLength] = '\0'; // 手动添加字符串结束符
        strncpy(value, ptr + 2, strlen(ptr + 2) - 1);
        value[strlen(ptr + 2) - 1] = '\0';
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
            printf("alias %s='%s'\n", aliases[i].Key, aliases[i].Value);
        }
    }
    else {
        // 有参数，依次处理每一个参数
        for (int i = 1;args[i] != NULL;i++) {
            //检查参数中有无等号
            // char AKey[LEN_ALIAS]; //键
            // char AValue[LEN_ALIAS]; //值
            char* AKey = (char*)malloc(strlen(args[i]) * sizeof(char)); //键
            char* AValue = (char*)malloc(strlen(args[i]) * sizeof(char)); //值
            //有等号，添加别名
            if (ParseEquality(args[i], AKey, AValue) == 0) {
                if (findAlias(AKey) == -1)
                    addAlias(AKey, AValue);
                else updateAlias(AKey, AValue);
            }
            //没有等号，查询别名
            else {
                int result = findAlias(args[i]);
                if (result != -1) {
                    printf("alias %s='%s'\n", args[i], aliases[result].Value);
                }
                else printf("Can't find alias: %s\n", args[i]);
            }
            free(AKey);
            free(AValue);
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

//加载别名
void LoadAlias() {
    FILE* fp = fopen("./alias.txt", "a+");
    char* bufkey = NULL;
    char* bufval = NULL;
    size_t bufsizekey = 0;
    size_t bufsizeval = 0;
    int lineNO = 0;

    while (getline(&bufkey, &bufsizekey, fp) != -1) {
        if (getline(&bufval, &bufsizeval, fp) != -1) {
            bufkey[strlen(bufkey) - 1] = '\0';
            bufval[strlen(bufval) - 1] = '\0';
            if (findAlias(bufkey) == -1)
                addAlias(bufkey, bufval);
            else updateAlias(bufkey, bufval);
        }
    }
    free(bufkey);
    free(bufval);
    fclose(fp);
}

//保存别名
void SaveAlias() {
    FILE* fp = fopen("./alias.txt", "w");
    for (int i = 0; i < AliasCount; i++) {
        fprintf(fp, "%s\n%s\n", aliases[i].Key, aliases[i].Value);
    }
    fclose(fp);
}

//执行命令，包括管道命令和普通命令
void ExeCmd(char* args[]) {
    // 保存当前的文件描述符
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stdin = dup(STDIN_FILENO);

    int npid = 1; //需要创建子进程的数量
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
            }
            else if (strcmp(args[iargs], ">") == 0) {
                // 输出重定向（覆盖写）
                iargs++;
                int fd = open(args[iargs], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            else if (strcmp(args[iargs], ">>") == 0) {
                // 输出重定向（追加写）
                iargs++;
                int fd = open(args[iargs], O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            else {
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
        }
        else if (pid == 0) { //在子进程中
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
        }
        else { //父进程
            if (i > 0) { // 不是第一个命令
                close(pipefds[i - 1][0]);
                close(pipefds[i - 1][1]);
            }
            wait(NULL);

            // 恢复文件描述符
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
            dup2(saved_stdin, STDIN_FILENO);
        }

        if (args[iargs] != NULL) {
            iargs++; // 跳过管道符号
        }
    }
}


//检查参数中是否有未被替换过的别名，有则返回1，无则返回0
int existAlias(char** args) {
    for (int i = 0;args[i] != NULL;i++) {
        int AliasPos = findAlias(args[i]);
        if (AliasPos != -1) {
            if (args[i + 1] == NULL) //下一个参数为空，说明该别名还没有替换
                return 1;
            if (strstr(aliases[AliasPos].Value, args[i + 1]) == NULL) //下一个参数不为空，检查别名的值是否含有该参数
                return 1;
        }
    }
    return 0;
}

//替换别名，参数：原命令，替换别名后的命令
void ReplaceAlias(char* source) {
    int len = strlen(source);
    char* sourceargs[64]; //指向source中的每个参数
    int amount = parse(source, sourceargs, 0, 1);//不去除双引号，但是替换空格为\0，因为要保持result和source的长度一致
    if (strcmp(sourceargs[0], "unalias") != 0 && strcmp(sourceargs[0], "alias") != 0) { //不替换alias、unalias命令
        while (existAlias(sourceargs)) {
            //在result中逆向查找并替换别名，防止正向替换后下标位置改变不能正确定位
            for (int i = amount - 1;i >= 0;i--) {
                int AliasPos = findAlias(sourceargs[i]);
                int arglen = strlen(sourceargs[i]);
                if (i != amount - 1) *(sourceargs[i + 1] - 1) = ' '; //去掉结束符\0
                if (AliasPos != -1) { //如果这个命令有别名
                    strcpy(sourceargs[i] + strlen(aliases[AliasPos].Value), sourceargs[i] + arglen);
                    strncpy(sourceargs[i], aliases[AliasPos].Value, strlen(aliases[AliasPos].Value));
                }
            }
            amount = parse(source, sourceargs, 0, 1);
        }
    }
    //恢复命令开头被替换成\0的空格，确保字符串正确识别
    for(int i=0;i<len;i++){
        if(source[i]=='\0') source[i]=' ';
    }
    //恢复被替换成结束符\0的空格
    for (int i = amount - 1;i >= 0;i--) {
        if (i != amount - 1) *(sourceargs[i + 1] - 1) = ' '; //去掉结束符\0
    }
}

int main(void)
{
    aliases = (struct Aliases*)malloc(AliasCapacity * sizeof(struct Aliases));
    LoadAlias();
    //按下TAB自动补全
    rl_bind_key('\t', rl_complete);
    using_history();
    while (1)
    {
        //显示提示符
        char hostname[100] = { 0 };
        gethostname(hostname, sizeof(hostname));
        char shell_prompt[200];
        snprintf(shell_prompt, sizeof(shell_prompt), ANSI_COLOR_MAGENTA_BOLD "MyShell" ANSI_COLOR_GREEN_BOLD "%s@%s" ANSI_COLOR_RESET ":" ANSI_COLOR_BLUE_BOLD "%s" ANSI_COLOR_RESET "$ ", getenv("USER"), hostname, getcwd(NULL, 1024));

        // 获取用户输入
        char* buf;
        buf = readline(shell_prompt);
        while (buf == NULL || strcmp(buf, "") == 0) {
            buf = readline(shell_prompt);
        }
        add_history(buf);

        //替换别名
        //cmd是替换了别名后将要执行的命令
        char* cmd = (char*)malloc((100 + strlen(buf)) * sizeof(char));
        strcpy(cmd, buf);
        ReplaceAlias(cmd);


        // 解析输入，获取参数和参数的数量
        char* args[64]; //64个命令和参数
        int argnum = parse(cmd, args, 1, 1);

        if (strcmp(args[0], "exit") == 0)
            break;
        else if (strcmp(args[0], "ver") == 0)
            printf("MyShell version 1.0, written by Bolddriver. https://github.com/Bolddriver/MyShell\n");
        else if (strcmp(args[0], "alias") == 0)
            Alias(args);
        else if (strcmp(args[0], "unalias") == 0)
            UnAlias(args);
        else if (strcmp(args[0], "cd") == 0) //更改工作目录
            chdir(args[1]);
        else
        {
            ExeCmd(args);
        }
        free(buf);
        free(cmd);
    }
    SaveAlias();
    exit(0);
    return 0;
}