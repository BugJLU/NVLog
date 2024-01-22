#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    printf("synctest start\n");
    int fd = open("./synt", O_CREAT|O_RDWR);
    printf("fd %d\n", fd);
    for (int i = 0; i < 10000; i++)
    {
        write(fd, "abcdefgh", 8);
        fsync(fd);
    }
    write(fd, "\ndone\n", 7);
    close(fd);
    printf("synctest done\n");
    return 0;
}