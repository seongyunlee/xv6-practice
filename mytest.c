#include "types.h"
#include "user.h"
#include "stat.h"

int main(){
    printf("start test");
    mmap(1,2,3,4,6,7);
    uint x =0x4000000;
    printf(1,"%c",x);
    exit();
}
