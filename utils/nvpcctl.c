#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#define NVPC_NAME "libnvpc"
#define LIBNVPC_DEV_PATH "/dev/libnvpc"

#define LIBNVPC_IOCTL_BASE 'N'

typedef unsigned char u8;

#define LIBNVPC_IOC_INIT    _IOR(LIBNVPC_IOCTL_BASE, 0, u8*)  
#define LIBNVPC_IOC_FINI    _IOR(LIBNVPC_IOCTL_BASE, 1, u8*)

#define PATH_MAX 4096

static int ln_fd;

static void open_libnvpc()
{
    ln_fd = open(LIBNVPC_DEV_PATH, O_RDWR);
    if (ln_fd < 0)
    {
        fprintf(stderr, "Cannot open libnvpc device at %s.\n", LIBNVPC_DEV_PATH);
        exit(-1);
    }
    
}

static void close_libnvpc()
{
    close(ln_fd);
}

static void start_nvpc(char *path)
{
    int ret;
    if ((ret = ioctl(ln_fd, LIBNVPC_IOC_INIT, path)) < 0)
    {
        fprintf(stderr, "Libnvpc error: ioctl failed to initialize nvpc: %d\n", ret);
        exit(-1);
    }
}

static void stop_nvpc()
{
    int ret;
    if ((ret = ioctl(ln_fd, LIBNVPC_IOC_FINI, NULL)) < 0)
    {
        // this should not happen
        fprintf(stderr, "Libnvpc error: ioctl failed to finalize nvpc: %d\n", ret);
        exit(-1);
    }
}

static ssize_t read_nvpc(char *buf, size_t len, off64_t off)
{
    ssize_t ret;
    if ((ret = pread(ln_fd, buf, len, off)) < 0)
    {
        fprintf(stderr, "Libnvpc error: read failed, err: %d: %s\n", errno, strerror(errno));
        exit(-1);
    }
    return ret;
}

static ssize_t write_nvpc(char *buf, size_t len, off64_t off)
{
    ssize_t ret;
    if ((ret = pwrite(ln_fd, buf, len, off)) < 0)
    {
        fprintf(stderr, "Libnvpc error: write failed, err: %d: %s\n", errno, strerror(errno));
        exit(-1);
    }
    return ret;
}

int main(int argc, char *argv[])
{
    char nvm_dev_path[PATH_MAX];
    off64_t off;
    size_t len;
    char buf[256];
    int flag = -1;
    ssize_t ret;
    int set_flag; // for flush set
    char tmp[255];

    if (argc >= 2)
    {
        /* nvpcctl start <path> */
        if (!strcmp(argv[1], "start") && argc == 3)
        {
            strcpy(nvm_dev_path, argv[2]);
            flag = 1;
        }
        /* nvpcctl stop */
        else if (!strcmp(argv[1], "stop") && argc == 2)
        {
            flag = 2;
        }
        /* nvpcctl read <off> <len> */
        else if (!strcmp(argv[1], "read") && argc == 4)
        {
            off = strtoll(argv[2], NULL, 10);
            len = strtoll(argv[3], NULL, 10);
            // off = atol(argv[2]);
            // len = atol(argv[3]);
            if (len <= 255)
                flag = 3;
        }
        /* nvpcctl write <off> <string> */
        else if (!strcmp(argv[1], "write") && argc == 4)
        {
            // off = atol(argv[2]);
            off = strtoll(argv[2], NULL, 10);
            strcpy(buf, argv[3]);
            len = strlen(buf);
            if (len <= 255)
                flag = 4;
        }
        /* nvpcctl flush */
        else if (!strcmp(argv[1], "flush"))
        {
            /* nvpcctl flush show */
            if (argc == 3 && !strcmp(argv[2], "show"))
            {
                flag = 5;
            }
            /* nvpcctl flush set <0/1> */
            else if (argc == 4 && !strcmp(argv[2], "set"))
            {
                set_flag = atoi(argv[3]);
                flag = 6;
            }
            
        }
        
        
    }

    switch (flag)
    {
    case 1:
        printf("nvpcctl: invoking nvpc from %s\n", nvm_dev_path);
        open_libnvpc();
        start_nvpc(nvm_dev_path);
        close_libnvpc();
        printf("nvpcctl: nvpc start ok\n");
        break;
    case 2:
        printf("nvpcctl: nvpc releasing nvm\n");
        open_libnvpc();
        stop_nvpc();
        close_libnvpc();
        printf("nvpcctl: nvpc stop ok\n");
        break;
    case 3:
        printf("nvpcctl: read from nvpc offset: %jd, length: %zu\n", off, len);
        open_libnvpc();
        ret = read_nvpc(buf, len, off);
        close_libnvpc();
        printf("nvpcctl: nvpc read ok, %zd bytes read, result: %s\n", ret, buf);
        break;
    case 4:
        printf("nvpcctl: write to nvpc offset: %jd, length: %zu, data: %s\n", off, len, buf);
        open_libnvpc();
        ret = write_nvpc(buf, len, off);
        close_libnvpc();
        printf("nvpcctl: nvpc write ok, %zd bytes written\n", ret);
        break;
    case 5:
        printf("nvpcctl: nvpc flush state: ");
        system("cat /sys/module/libnvpc/parameters/nvpc_flush");
        break;
    case 6:
        sprintf(tmp, "echo %d > /sys/module/libnvpc/parameters/nvpc_flush", set_flag);
        printf("nvpcctl: running cmd: %s\n", tmp);
        system(tmp);
        printf("nvpcctl: nvpc flush set state to: %d\n", set_flag);
        break;
    default:
        printf(
            "usage: \n"
            "\tnvpcctl start <dev_path>\n"
            "\tnvpcctl stop\n"
            "\tnvpcctl read <off> <len>:\tlen<=255\n"
            "\tnvpcctl write <off> <string>:\tlen(string)<=255\n"
            "\tnvpcctl flush show\n"
            "\tnvpcctl flush set <0/1>\n"
        );
        break;
    }
    
    
}