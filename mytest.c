#include "types.h"
#include "user.h"
#include "stat.h"
#include "param.h"
#include "fcntl.h"

int main(){
    printf(1,"start test\n");
    uint i = (uint) mmap(0, 8192, PROT_READ, MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    printf(1,"%d %x\n",*((int *)i));
    int* x = (int *)mmap(8192,4096,PROT_READ|PROT_WRITE,MAP_ANONYMOUS,-1,0);
    int fd;
    for(int i=0;i<1024;i++){
        x[i]=i;
    }
    for(int k=0;k<1024;k++){
        printf(1,"%d",x[k]);
    }


    if((fd = open("abcdef", O_CREATE|O_RDWR))==-1){
        printf(1,"fail\n");
    }
    else{
        printf(1,"success\n");
        char arr[32] = "ABCDABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNO" //32byte arr
        for(int i=0;i<128;i++){
            putc(fd,arr); //32byte string
        }
        printf(1,"write done\n");
    }

    char* fp = (char *)mmap(16384, 4096, PROT_READ, MAP_POPULATE, fd, 0); 

    printf(1,"print %s\n",fp);
    for(char* i=fp;i<&fp[4096];i++){
        printf(1,"<memory read from %x %c >",(int)i,&i);
    }

    printf(1,"\n");
    exit();
}
