#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FILE_PATH "./file/bigfile"
#define BYTE_OF_PAGE 4096

int main()
{
    int flag = O_RDONLY;
    int mode = 0;
    int fd = open(FILE_PATH, flag);
    if (fd == -1)
    {
        printf("open failed\n");
        return 0;
    }
    char charater[BYTE_OF_PAGE];
    int count = 0;
    int number = 0;
    while (1)
    {
        ssize_t res = read(fd, charater, BYTE_OF_PAGE);
        if (res == -1)
        {
            printf("read failed\n");
        }
        else if (res == 0)
        {
            printf("end of file\n");
            break;
        }
        else
        {
            count++;
            if (count == 1024)
            {
                count = 0;
                number++;
                printf("%d 4MB read\n", number);
            }
        }
    }
    close(fd);

    return 0;
}