#include <stdio.h>
#include <stdlib.h>


#include <sys/types.h>//这里提供类型pid_t和size_t的定义
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> 

#define mb 1024*1024

char _MB[mb]={};

int stoi(char *str)//字符串转数字 
{
    char flag = '+';//指示结果是否带符号 
    long res = 0;
    
    if(*str=='-')//字符串带负号 
    {
        ++str;//指向下一个字符 
        flag = '-';//将标志设为负号 
    } 
    //逐个字符转换，并累加到结果res 
    while(*str>=48 && *str<=57)//如果是数字才进行转换，数字0~9的ASCII码：48~57 
    {
        res = 10*res+  *str++-48;//字符'0'的ASCII码为48,48-48=0刚好转化为数字0 
    } 
 
    if(flag == '-')//处理是负数的情况
    {
        res = -res;
    }
 
    return (int)res;
}


int main(int argc, char *argv[]){
    //解析入参
    int n=stoi(argv[2]);
    char *filename=argv[1];

    if(argc!=3){
        printf("wrong parameter\n");
        return 0;
    }

    //单位是MB

    int fd;

    printf("ready to write %d GB data\n",n);
    
    fd=open(filename,O_RDWR|O_CREAT);

    for(int i=0;i<n*1024;i++){
        write(fd,_MB,mb);
        printf("write %dMB:%dMB\n",i+1,n*1024);
    }

    close(fd);

    printf("ready to read %d GB data\n",n);

    fd=open(filename,O_RDWR);

    for(int i=0;i<n*1024;i++){
        read(fd,_MB,mb);
        printf("read %dMB:%dMB\n",i+1,n*1024);
    }

    close(fd);

    return 0;
}