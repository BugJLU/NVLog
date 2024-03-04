#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdbool.h>

#define BUFFER_SIZE 16

void print_error(const char *msg) {
    printf("%s\n", msg);
}

bool isInteger(const char *str) {
    if (str == NULL || *str == '\0') {
        return false;
    }

    char *endptr;
    strtol(str, &endptr, 10);

    if (*endptr != '\0') {
        return false;  // 无法完全转换为整数
    }

    return true;
}

int main(int argc,char *args[]) {//     iter  totalsize(GB)   opsize(KB) 
    if(argc!=4){
        printf("wrong argument count\n");
        return 1;
    }
    if(isInteger(args[1])==false||isInteger(args[2])==false||isInteger(args[3])==false){
        printf("wrong argument format\n");
        return 1;
    }
    long int iter = atoi(args[1]);
    long int totalsize = atoi(args[2]);
    long int file_size_b;
    file_size_b = totalsize * 1024 * 1024 * 1024;// 将文件大小从GB转换为B
    if(totalsize==0){
        file_size_b=1*1024*1024;//1MB
    }
    long int opsize = atoi(args[3]);
    char *file_path="/mnt/50GB";  
    char *data = (char *)malloc(BUFFER_SIZE * 1024 * sizeof(char));

    // 循环更新文件内容
    for (int i = 0; i < iter; i++) {
        int fd = open(file_path, O_RDWR);
        if (fd == -1) {
            print_error("Failed to open file");
            return 1;
        }
        off_t offset = 0;
        while (offset < file_size_b) {
            if (lseek(fd, offset, SEEK_SET) == -1) {
                print_error("lseek failed");
                return 1;
            }
            if (read(fd, data, opsize * 1024) == -1) {
                print_error("read failed");
                return 1;
            }

            // 修改数据
            for (int j = 0; j < opsize * 1024; j++) {
                data[j] = 'a';
            }

            if (lseek(fd, offset, SEEK_SET) == -1) {
                print_error("lseek failed");
                return 1;
            }
            
            if (write(fd, data, opsize * 1024) == -1) {
                print_error("write failed");
                return 1;
            }

            if (fsync(fd) == -1) {
                print_error("fsync failed");
                return 1;
            }
            if(offset%1048576==0){
                printf("offset/totalsize : %lu/%lu round %d\n",offset,file_size_b,i);
            }
            offset += opsize * 1024;
        }
        close(fd);
    }

    free(data);
    return 0;
}