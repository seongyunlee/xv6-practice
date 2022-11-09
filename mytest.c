#include "types.h"
#include "user.h"
#include "stat.h"
#include "param.h"
#include "fcntl.h"

int main(){
    printf(1,"start test\n");

    printf(1,"free space first %d\n",freemem());
    int* x = (int *)mmap(0,4096,PROT_READ|PROT_WRITE,MAP_ANONYMOUS,-1,0);
    int fd;
    printf(1,"free space annonymous map %d\n",freemem());
    for(int i=0;i<1024;i++){
        x[i]=i;
    }

    printf(1,"free space after annoymous lazy access %d\n",freemem());
    for(int k=0;k<1024;k++){
        printf(1,"%d",x[k]);
    }
    printf(1,"\n%d : anonymous mmap test done\n",getpid());


    if((fd = open("abcdef", O_CREATE|O_RDWR))==-1){
        printf(1,"fail\n");
    }
    else{
        printf(1,"success\n");
        for(int i=0;i<1;i++){
            printf(fd,"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFABCDEFGHIJKLMNOPQRSTUVWXYZABCDEF"); //64byte string
        }
        printf(1,"write done\n");
    }
    printf(1,"%d process is runnning\n",getpid());
      fork();
    wait();
        printf(1,"free space before file mmap  %d\n",freemem());


    char *mmap_addr = (char*)mmap(4096, 8192, PROT_READ|PROT_WRITE, 0, fd, 1);
  
    printf(1,"can access %x %d\n",(int)x,x[0]);
    printf(1,"%d :mmap file success va%x\n",getpid(),(int)mmap_addr);
    
    //printf(1,"free space after file mmap %d\n",freemem());
    for(int i=0;i<64;i++){
        printf(1,"<memory read from %x %c >\n",(int)mmap_addr+i,mmap_addr[i]);
    }
            printf(1,"free space before file mmap  %d\n",freemem());

    printf(1,"\n\n munmap test\n");
    printf(1,"this massage should be printed %c %x\n",mmap_addr[0],(int)mmap_addr);
    mmap_addr[0]=3;
    munmap((uint)mmap_addr);
    printf(1,"free space after unmmap %d\n",freemem());
    printf(1,"this massage should not be printed %x %x\n",mmap_addr[0],(int)mmap_addr);
    printf(1,"\n");

    
    exit();
}
