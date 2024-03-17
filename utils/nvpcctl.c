#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#define NVPC_NAME "libnvpc"
#define LIBNVPC_DEV_PATH "/dev/libnvpc"

#define LIBNVPC_IOCTL_BASE 'N'

typedef unsigned char u8;

#define LIBNVPC_IOC_INIT    _IOR(LIBNVPC_IOCTL_BASE, 0, u8*)  
#define LIBNVPC_IOC_FINI    _IOR(LIBNVPC_IOCTL_BASE, 1, u8*)
#define LIBNVPC_IOC_USAGE   _IOR(LIBNVPC_IOCTL_BASE, 2, u8*)
#define LIBNVPC_IOC_TEST    _IOR(LIBNVPC_IOCTL_BASE, 3, u8*)
#define LIBNVPC_IOC_OPEN    _IOR(LIBNVPC_IOCTL_BASE, 4, u8*)
#define LIBNVPC_IOC_CLOSE   _IOR(LIBNVPC_IOCTL_BASE, 5, u8*)
#define LIBNVPC_IOC_CONFIG  _IOR(LIBNVPC_IOCTL_BASE, 6, u8*)

#define PATH_MAX 4096

typedef struct nvpc_usage_s
{
    size_t nvpc_pgs;
    size_t free_pgs;
    size_t syn_used;

} nvpc_usage_t;

struct nvpc_opts
{
    // struct dax_device *dev;
    // int nid;        /* numa node id */
    void *__reserved1;
    int __reserved2;
    bool extend_lru;
    bool absorb_syn;
    bool nvpc_lru_evict;
    u8 promote_level;
    bool demote_before_promote;
    size_t nvpc_sz;  /* in pages */
    // size_t syn_sz;  /* in pages */
    bool force; // force to initialize
    bool rebuild; // rebuild the nvpc data in NVM
    // bool __prepared;    // prepared before init, only used by dax driver
    bool __reserved3;
};


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

static void open_nvpc_onsb(char *path, int mode)
{
    int ret;
    struct open_s {
        char *path;
        int mode;
    } openarg = {
        .path = path, 
        .mode = mode,
    };
    if ((ret = ioctl(ln_fd, LIBNVPC_IOC_OPEN, &openarg)) < 0)
    {
        fprintf(stderr, "Libnvpc error: ioctl failed to enable nvpc for fs: %d\n", ret);
        exit(-1);
    }
}

static void close_nvpc_onsb(char *path)
{
    int ret;
    if ((ret = ioctl(ln_fd, LIBNVPC_IOC_CLOSE, path)) < 0)
    {
        fprintf(stderr, "Libnvpc error: ioctl failed to disable nvpc for fs: %d\n", ret);
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

static void get_nvpc_usage(nvpc_usage_t *usage)
{
    int ret;
    if ((ret = ioctl(ln_fd, LIBNVPC_IOC_USAGE, usage)) < 0)
    {
        // this should not happen
        fprintf(stderr, "Libnvpc error: ioctl failed to get nvpc usage: %d\n", ret);
        exit(-1);
    }
}

static void nvpc_test()
{
    int ret;
    if ((ret = ioctl(ln_fd, LIBNVPC_IOC_TEST, 0)) < 0)
    {
        // this should not happen
        fprintf(stderr, "Libnvpc error: ioctl failed\n");
        exit(-1);
    }
}

static void nvpc_test1(char *path, size_t len, char *tmp1)
{
    int ret;
    struct test1_s {
        char *path;
        size_t len;
        char *tmp1;
    } test1 = {
        .path = path, 
        .len = len,
        .tmp1 = tmp1,
    };
    if ((ret = ioctl(ln_fd, LIBNVPC_IOC_TEST, &test1)) < 0)
    {
        // this should not happen
        fprintf(stderr, "Libnvpc error: ioctl failed\n");
        exit(-1);
    }
}

static void set_nvpc_config(struct nvpc_opts *opts)
{
    int ret;
    if ((ret = ioctl(ln_fd, LIBNVPC_IOC_CONFIG, opts)) < 0)
    {
        // this should not happen
        fprintf(stderr, "Libnvpc error: ioctl failed\n");
        exit(-1);
    }
}

static char splits[] = {' ', '\t', '\n', '\0', '='};

static char *find_split(char *str)
{
    char *t = str;
    while (*t)
    {
        for (size_t i = 0; i < sizeof(splits); i++)
        {
            if (*t == splits[i])
            {
                return t;
            }
        }
        t++;
    }
    return t;
}

static char *__opts_titles[] = {
    "extend_lru", "absorb_syn", "promote_level", "demote_before_promote", 
    "nvpc_lru_evict", "nvpc_sz", "rebuild", "force_start"
};
static int __opts_titles_sz = 8;

static int __nvpc_set_opt(struct nvpc_opts *opts, char *title, char *value)
{
    int i;
    int val;
    for (i = 0; i < __opts_titles_sz; i++)
    {
        if (!strcmp(title, __opts_titles[i]))
        {
            break;
        }
    }
    val = atoi(value);
    switch (i)
    {
    case 0:
        opts->extend_lru = (bool)val;
        break;
    case 1:
        opts->absorb_syn = (bool)val;
        break;
    case 2:
        opts->promote_level = (u8)val;
        break;
    case 3:
        opts->demote_before_promote = (bool)val;
        break;
    case 4:
        opts->nvpc_lru_evict = (bool)val;
        break;
    case 5:
        opts->nvpc_sz = (size_t)val;
        break;
    case 6:
        opts->rebuild = (bool)val;
        break;
    case 7:
        opts->force = (bool)val;
        break;
    
    default:
        return -1;
        break;
    }
    return 0;
}

static int nvpc_prepare_opts(struct nvpc_opts *opts, int argc, char *argv[])
{
    char *this, *head, *title, *value, *tmp;
    for (int i = 0; i < argc; i++)
    {
        tmp = head = this = strdup(argv[i]);
        while (*this)
        {
            title = value = this;
            value=find_split(value);
            while (*value == ' ')
            {
                *value = 0;
                value++;
            }
            
            if (*value != '=')
            {
                free(head);
                printf("ExpErr1\n");
                return i+2;
            }
            *value = 0;
            value++;
            tmp = find_split(value);
            while (*tmp == ' ' || *tmp == '\t' || *tmp == '\n')
            {
                *tmp = 0;
                tmp++;
            }
            if (*tmp == '=')
            {
                free(head);
                printf("ExpErr2\n");
                return i+2;
            }
            else
            {
                this = tmp;
            }
            if (__nvpc_set_opt(opts, title, value))
            {
                free(head);
                printf("ExpErr3 title %s value %s; \n", title, value);
                return i+2;
            }
            
            printf("%s - %s; \n", title, value);
        }

        free(head);
    }
    
    return 0;
}

int main(int argc, char *argv[])
{
    char nv_path[PATH_MAX];
    off64_t off;
    size_t len;
    char buf[256];
    int flag = -1;
    ssize_t ret;
    int set_flag; // for flush set
    char tmp[255];
    nvpc_usage_t usage;
    char *tmp1;
    struct nvpc_opts opts;

    if (argc >= 2)
    {
        /* nvpcctl start <path> */
        if (!strcmp(argv[1], "start") && argc == 3)
        {
            strcpy(nv_path, argv[2]);
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
        /* nvpcctl wbarrier */
        /* wbarrier only works when flush is set to 1 */
        else if (!strcmp(argv[1], "wbarrier"))
        {
            /* nvpcctl wbarrier show */
            if (argc == 3 && !strcmp(argv[2], "show"))
            {
                flag = 7;
            }
            /* nvpcctl wbarrier set <0/1> */
            else if (argc == 4 && !strcmp(argv[2], "set"))
            {
                set_flag = atoi(argv[3]);
                flag = 8;
            }
        }
        /* nvpcctl usage */
        else if (!strcmp(argv[1], "usage"))
        {
            flag = 9;
        }
        /* nvpcctl test */
        else if (!strcmp(argv[1], "test"))
        {
            flag = 10;
        }
        /* nvpcctl open <path> */
        else if (!strcmp(argv[1], "open"))
        {
            if (argc == 3)
            {
                strcpy(nv_path, argv[2]);
                set_flag = 1;   /* default strict */
                flag = 11;
            } 
            else if (argc == 4)
            {
                strcpy(nv_path, argv[2]);
                flag = 11;
                if (!strcmp(argv[3], "r") || !strcmp(argv[3], "relaxed"))
                    set_flag = 0;
                else if (!strcmp(argv[3], "s") || !strcmp(argv[3], "strict"))
                    set_flag = 1;
                else
                    flag = 999;
            }
        }
        /* nvpcctl close <path> */
        else if (!strcmp(argv[1], "close") && argc == 3)
        {
            strcpy(nv_path, argv[2]);
            flag = 12;
        }
        /* nvpcctl test1 <path> <len> */
        else if (!strcmp(argv[1], "test1"))
        {
            strcpy(nv_path, argv[2]);
            len = strtoll(argv[3], NULL, 10);
            tmp1 = (char*)malloc(len);
            memset(tmp1, 't', len);
            flag = 101;
        }
        /* nvpcctl activesync set */
        else if (!strcmp(argv[1], "activesync"))
        {
            /* nvpcctl activesync show */
            if (argc == 3 && !strcmp(argv[2], "show"))
            {
                flag = 13;
            }
            /* nvpcctl activesync set <0/1> */
            else if (argc == 4 && !strcmp(argv[2], "set"))
            {
                set_flag = atoi(argv[3]);
                flag = 14;
            }
        }
        else if (!strcmp(argv[1], "config"))
        {
            ret = nvpc_prepare_opts(&opts, argc-2, argv+2);
            if (!ret)
            {
                flag = 20;
            }
            else
            {
                printf("Cannot resolve config %s. \n", argv[ret]);
            }
            
        }
    }

    switch (flag)
    {
    case 1:
        printf("nvpcctl: start is discarded\n");
        break;
        printf("nvpcctl: invoking nvpc from %s\n", nv_path);
        open_libnvpc();
        start_nvpc(nv_path);
        close_libnvpc();
        printf("nvpcctl: nvpc start ok\n");
        break;
    case 2:
        printf("nvpcctl: stop is discarded\n");
        break;
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
        printf("nvpcctl: nvpc flush state: \n");
        system("cat /sys/module/libnvpc/parameters/nvpc_flush");
        break;
    case 6:
        sprintf(tmp, "echo %d > /sys/module/libnvpc/parameters/nvpc_flush", set_flag);
        printf("nvpcctl: running cmd: %s\n", tmp);
        system(tmp);
        printf("nvpcctl: nvpc flush set state to: %d\n", set_flag);
        break;
    case 7:
        printf("nvpcctl: nvpc wbarrier state: \n");
        system("cat /sys/module/libnvpc/parameters/nvpc_wbarrier");
        break;
    case 8:
        sprintf(tmp, "echo %d > /sys/module/libnvpc/parameters/nvpc_wbarrier", set_flag);
        printf("nvpcctl: running cmd: %s\n", tmp);
        system(tmp);
        printf("nvpcctl: nvpc wbarrier set state to: %d\n", set_flag);
        break;
    case 9:
        open_libnvpc();
        get_nvpc_usage(&usage);
        close_libnvpc();
        // printf("nvpcctl: lru usage: \t%ld \tof %ld \tpages free\n", usage.lru_free, usage.lru_sz);
        // printf("nvpcctl: syn usage: \t%ld \tof %ld \tpages free\n", usage.syn_free, usage.syn_sz);
        printf("nvpcctl: total\t%ld pages\n", usage.nvpc_pgs);
        printf("nvpcctl: used\t%ld pages\n", usage.nvpc_pgs-usage.free_pgs);
        printf("nvpcctl: syn used\t%ld pages\n", usage.syn_used);
        printf("nvpcctl: free\t%ld pages\n", usage.free_pgs);
        break;
    case 10:
        open_libnvpc();
        nvpc_test();
        close_libnvpc();
        break;
    case 11:
        printf("nvpcctl: enabling nvpc on fs @ %s\n", nv_path);
        open_libnvpc();
        open_nvpc_onsb(nv_path, set_flag);
        close_libnvpc();
        printf("nvpcctl: done\n");
        break;
    case 12:
        printf("nvpcctl: disabling nvpc on fs @ %s\n", nv_path);
        open_libnvpc();
        close_nvpc_onsb(nv_path);
        close_libnvpc();
        printf("nvpcctl: done\n");
        break;
    case 13:
        printf("nvpcctl: nvpc activesync state: \n");
        system("cat /sys/module/libnvpc/parameters/nvpc_active_sync");
        break;
    case 14: 
        sprintf(tmp, "echo %d > /sys/module/libnvpc/parameters/nvpc_active_sync", set_flag);
        printf("nvpcctl: running cmd: %s\n", tmp);
        system(tmp);
        printf("nvpcctl: nvpc activesync set state to: %d\n", set_flag);
        break;
    case 20:
        printf("nvpcctl: setting config\n");
        open_libnvpc();
        set_nvpc_config(&opts);
        close_libnvpc();
        printf("nvpcctl: done\n");
        break;
    case 101:
        open_libnvpc();
        nvpc_test1(nv_path, len, tmp1);
        close_libnvpc();
        free(tmp1);
        break;
    default:
        printf(
            "usage: \n"
            "\tnvpcctl start <dev_path> (discarded)\n"
            "\tnvpcctl stop (discarded)\n"
            "\tnvpcctl read <off> <len>:\tlen<=255\n"
            "\tnvpcctl write <off> <string>:\tlen(string)<=255\n"
            "\tnvpcctl flush show\n"
            "\tnvpcctl flush set <0/1>\n"
            "\tnvpcctl wbarrier show\n"
            "\tnvpcctl wbarrier set <0/1>\n"
            "\tnvpcctl usage\n"
            "\tnvpcctl open <path>\n"
            "\tnvpcctl open <path> <r/relaxed/s/strict>\n"
            "\tnvpcctl close <path>\n"
            "\tnvpcctl activesync show\n"
            "\tnvpcctl activesync set <0/1>\n"
            // TODO:
            "\tnvpcctl config set <option=value> ...:\tsetup config before init\n"
            "config options:\n"
            "\textend_lru=<0/1>\n"
            "\tabsorb_syn=<0/1>\n"
            "\tpromote_level=<u8>\n"
            "\tdemote_before_promote=<0/1>\n"
            "\tnvpc_lru_evict=<0/1>\n"
            "\tnvpc_sz=<int>:\t-1 means total NVM size\n"
            "\trebuild=<0/1>\n"
            "\tforce_start=<0/1>\n"
        );
        break;
    }
    
    
}