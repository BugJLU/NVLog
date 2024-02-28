#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

void modifyFile(const char *filename, long totalSize, long blockSize) {
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    int numBlocks = totalSize / blockSize;

    char *buffer = (char *)malloc(blockSize);

    int *visited = (int *)malloc(numBlocks * sizeof(int));
    memset(visited, 0, numBlocks * sizeof(int));

    srand(time(NULL));

    int blocksVisited = 0;
    while (blocksVisited < numBlocks) {
        int blockIndex = rand() % numBlocks;
        if (visited[blockIndex] == 0) {
            visited[blockIndex] = 1;

            // 计算偏移量
            off_t offset = blockIndex * blockSize;

            // 移动文件指针
            lseek(fd, offset, SEEK_SET);

            // 读取数据块
            read(fd, buffer, blockSize);

            // 修改数据块内容
            for (int j = 0; j < blockSize; j++) {
                buffer[j] = ~buffer[j];
            }

            // 将修改后的数据块写回原处
            lseek(fd, offset, SEEK_SET);
            write(fd, buffer, blockSize);
            if (fsync(fd) == -1) {
                print_error("fsync failed");
                return 1;
            }

            blocksVisited++;
        }
    }

    free(buffer);
    free(visited);

    close(fd);
}

int main(int argc,char *args[]) {
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
    long int blockSize=0;
    long int totalSize=0;
    modifyFile("your_filename", totalSize ,blockSize); // 文件名和文件大小（以字节为单位）

    return 0;
}