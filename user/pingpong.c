#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[])
{
    int p[2], p1[2];
    int len = 4;
    pipe(p);
    pipe(p1);
    int ret = fork();
    if (ret < 0)
    {
        printf("Failed to create new process");
        exit(1);
    }
    else if (ret > 0)
    {
        close(p[0]);
        close(p1[1]);
        char *str1 = malloc(len * sizeof(char));
        str1 = "ping";
        write(p[1], str1, len);
        close(p[1]);
        char *str3 = malloc(len * sizeof(char));
        read(p1[0], str3, len);
        printf("%d: received %s\n", getpid(), str3);
    }
    else if (ret == 0)
    {
        close(p[1]);
        close(p1[0]);
        char *str2 = malloc(len * sizeof(char));
        read(p[0], str2, len);
        close(p[0]);
        printf("%d: received %s\n", getpid(), str2);
        char *str4 = malloc(len * sizeof(char));
        str4 = "pong";
        write(p1[1], str4, len);
    }
    exit(0);
}
