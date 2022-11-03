#include "types.h"
#include "user.h"
#include "stat.h"
#include "param.h"

int main(){
    printf(1,"start test\n");
    uint i = (uint) mmap(0, 8192, PROT_READ, MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    printf(1,"%d %x",*((int *)i));
    int* x = (int *) map(8192,4096,PROT_READ|PROT_WRITE,MAP_ANONYMOUS,-1,0);
    for(int i=0;i<1024;i++){
        x[i]=i;
    }
    for(int k=0;k<1024;k++){
        printf(1,"%d",x[k]);
    }
    printf(1,"\n");
    exit();
}
