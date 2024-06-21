#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// #define ROUND 100000    // keep it running

int main()
{
    char c = 'a';
    char pg[4096];
    off_t offset = 0;
    int fd = open("./cont", O_CREAT|O_RDWR);

    while (1)
    {
        memset(pg, c, 4096);

        pwrite(fd, pg, 4096, offset);
        fsync(fd);

        if (++c > 'z')
        {
            c = 'a';
        }
        offset++;
    }
    close(fd);
    
}