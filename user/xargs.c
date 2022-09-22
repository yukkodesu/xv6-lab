#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#define NULL 0
#define MAXARG_LENGTH 128

char *strtok(char *s, char d)
{
    static char *input = NULL;
    if (s != NULL)
        input = s;
    if (input == NULL)
        return NULL;
    char *result = malloc(strlen(input) + 1);
    int i = 0;
    for (; input[i] != '\0'; i++)
    {
        if (input[i] != d)
            result[i] = input[i];
        else
        {
            result[i] = '\0';
            input = input + i + 1;
            return result;
        }
    }
    result[i] = '\0';
    input = NULL;
    return result;
}

int main(int argc, char *argv[])
{
    char **args = malloc(MAXARG * sizeof(char *));
    for (int i = 0; i < MAXARG; i++)
    {
        args[i] = malloc(MAXARG_LENGTH * sizeof(char));
    }
    int p = 1;
    char cmd[512];
    strcpy(cmd, argv[1]);
    strcpy(args[0], argv[1]);
    for (int i = 2; i < argc; ++i)
    {
        strcpy(args[p++], argv[i]);
    }

    char str_stdin[128];
    while (gets(str_stdin, 128) && *str_stdin != 0)
    {
        char *str_end = str_stdin + strlen(str_stdin) - 1, *token;
        char s = ' ';
        *str_end = '\0';
        // printf("-%s\n", str_stdin);
        token = strtok(str_stdin, s);
        while (token != NULL)
        {
            strcpy(args[p++], token);
            token = strtok(NULL, s);
        }
    }

    args[p] = 0;
    int ret = fork();
    if (ret < 0)
    {
        printf("xargs: error to exec command %s\n", cmd);
    }
    else if (ret == 0)
    {
        exec(cmd, args);
    }
    else if (ret > 0)
    {
        int status;
        wait(&status);
    }
    exit(0);
}
