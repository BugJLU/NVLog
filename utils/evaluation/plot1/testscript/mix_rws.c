#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define PATH_MAX 4096

#define FSIZE (4096ul*1024*1024)    // 4GB file
#define IOALL (2048ul*1024*1024)    // 2GB total io
#define IOSZ 4096               // 4KB io size
#define IONUM (IOALL/IOSZ)
#define FIONUM (FSIZE/IOSZ)

char zeros[32768];

#define RD 0
#define WR 1

int rw01[10] = {WR,WR,WR,WR,WR,WR,WR,WR,WR,WR};
int rw37[10] = {RD,WR,WR,RD,WR,WR,RD,WR,WR,WR};
int rw55[10] = {RD,WR,RD,WR,RD,WR,RD,WR,RD,WR};
int rw73[10] = {RD,RD,WR,RD,RD,WR,RD,RD,WR,RD};

char *usage_info = "usage: test01_mixrws [dir_path] [rw_ratio] [sync_in_10]\n"
    "\trw_ratio: 01/37/55/73\n"
    "\tsync_in_10: 0~10";


int main(int argc, char *argv[])
{
    int *rwseq;
    int rw_ratio;
    int sync_in_10;
    char *ep;
    char filename[PATH_MAX];
    int pathlen;

    /* arguments parsing */
    
    if (argc != 4)
    {
        printf("wrong arg num %d. \n", argc);
        printf("%s\n", usage_info);
        return 0;
    }

    strcpy(filename, argv[1]);
    pathlen = strlen(argv[1]);
    if (filename[pathlen-1] != '/')
    {
        filename[pathlen] = '/';
        pathlen++;
    }
    strcpy(filename+pathlen, "test01_workfile");

    rw_ratio = strtol(argv[2], &ep, 10);
    if (*ep != '\0')
    {
        printf("wrong arg rw_ratio %s. \n", argv[2]);
        printf("%s\n", usage_info);
        return 0;
    }
    
    sync_in_10 = strtol(argv[3], &ep, 10);
    if (*ep != '\0')
    {
        printf("wrong arg sync_in_10 %s. \n", argv[3]);
        printf("%s\n", usage_info);
        return 0;
    }

    switch (rw_ratio)
    {
    case 1:
        rwseq = rw01;
        break;
    case 37:
        rwseq = rw37;
        break;
    case 55:
        rwseq = rw55;
        break;
    case 73:
        rwseq = rw73;
        break;
    default:
        printf("wrong arg rw_ratio %d. \n", rw_ratio);
        printf("%s\n", usage_info);
        return 0;
        break;
    }

    if (sync_in_10 < 0 || sync_in_10 > 10)
    {
        printf("wrong arg sync_in_10 %d. \n", sync_in_10);
        printf("%s\n", usage_info);
        return 0;
    }

    

    printf("mixrws test (rw_ratio=%02d, sync_in_10=%d, fname=%s)\n", rw_ratio, sync_in_10, filename);

    /* create file here */

    int fd;
    ssize_t bytes_left;
    memset((void*)zeros, 0, sizeof(zeros));
    printf("creating file with %lu bytes...\n", FSIZE);
    fd = open(filename, O_CREAT|O_RDWR);
    if (fd<0)
    {
        printf("cannot create work file %s.\n", filename);
        return -1;
    }
    bytes_left = FSIZE;
    while (bytes_left)
    {
        write(fd, zeros, sizeof(zeros));
        bytes_left -= sizeof(zeros);
    }
    bytes_left = FSIZE;
    /* make it hot */
    while (bytes_left)
    {
        read(fd, zeros, sizeof(zeros));
        bytes_left -= sizeof(zeros);
    }
    close(fd);

    /* rw work here */

    struct timespec start, end;
    unsigned long long duration_ms;
    double duration_s;

    int fds;
    int wr_counter = 0;
    int total_io = 0;
    char buf[IOSZ];
    off_t off;
    bytes_left = IOALL;

    fd = open(filename, O_RDWR);
    if (fd<0)
    {
        printf("cannot open work file %s.\n", filename);
        return -1;
    }
    fds = open(filename, O_RDWR|O_SYNC);
    if (fds<0)
    {
        printf("cannot open work file %s with O_SYNC.\n", filename);
        close(fd);
        return -1;
    }
    srand(317); // whatever, but don't use time

    printf("running test...\n");

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    while (bytes_left > 0)
    {
        for (int i = 0; i < 10; i++)
        {
            if (bytes_left <= 0)
                break;
            
            off = rand() % FIONUM;
            switch (rwseq[i])
            {
            case RD:
                // printf("rd fd=%d sz=%lu, off=%lu\n", fd, IOSZ, off);
                pread(fd, buf, IOSZ, off*IOSZ);
                break;
            case WR:
                if (wr_counter < sync_in_10)
                    // printf("wr_s fd=%d sz=%lu, off=%lu\n", fd, IOSZ, off);
                    pwrite(fds, zeros, IOSZ, off*IOSZ);
                else
                    // printf("wr fd=%d sz=%lu, off=%lu\n", fd, IOSZ, off);
                    pwrite(fd, zeros, IOSZ, off*IOSZ);
                wr_counter++;
                wr_counter %= 10;
                break;
            default:
                break;
            }
            bytes_left -= IOSZ;
            total_io++;
            // printf("io=%ld bytes_left=%ld \n", total_io, bytes_left);
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    close(fd);
    close(fds);

    duration_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
    duration_s = duration_ms / 1000.0;
    double total_MB = (double)total_io * IOSZ / 1024 / 1024;
    printf("time: %.2lf s; total: %.2lf MB; IOs: %d; thp: %.2lf iops; bw: %.2lf MB/s\n", duration_s, total_MB, total_io, total_io/duration_s, total_MB/duration_s);
    printf("done.\n");
    return 0;
}
