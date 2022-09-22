#include "kernel/types.h"
#include "user.h"
#define MAX_NUM 35

void getPrime(int p)
{
    int prime;
    if (read(p, &prime, sizeof(int)) <= 0)
        return;
    printf("prime %d\n", prime);
    int p1[2];
    pipe(p1);
    // printf("fork\n");
    int ret = fork();
    if (ret < 0)
    {
        printf("error at %d", prime);
        exit(1);
    }
    else if (ret > 0)
    {
        close(p1[0]);
        int n;
        while (read(p, &n, sizeof(int)) > 0)
        {
            // printf("get %d\n", n);
            if (n % prime)
            {
                // printf("put %d\n", n);
                write(p1[1], &n, sizeof(int));
            }
        }
        close(p1[1]);
        close(p);
        // printf("pipe close\n");
        int status;
        wait(&status);
    }
    else if (ret == 0)
    {
        close(p1[1]);
        getPrime(p1[0]);
    }
    return;
}

int main(int argc, char *argv[])
{
    int ret;
    int p0[2];
    pipe(p0);

    ret = fork();
    if (ret < 0)
    {
        printf("Error\n");
        exit(1);
    }
    else if (ret > 0)
    {
        close(p0[0]);
        for (int i = 2; i <= MAX_NUM; ++i)
        {
            write(p0[1], &i, sizeof(int));
        }
        close(p0[1]);
        int status;
        wait(&status);
    }
    else if (ret == 0)
    {
        close(p0[1]);
        getPrime(p0[0]);
    }
    exit(0);
}