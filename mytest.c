#include "types.h"
#include "user.h"
#include "stat.h"

int main(){
    printf(1,"start test\n");
    mmap(1,2,3,4,5,6);
    char  *x =0x4000000;
    printf(1,"%c",*x);
    printf(1,"done\n");
    exit();
}
