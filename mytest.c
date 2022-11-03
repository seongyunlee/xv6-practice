#include "types.h"
#include "user.h"
#include "stat.h"
#include "param.h"

int main(){
    printf(1,"start test\n");
    uint i = (uint) mmap(0, 8192, PROT_READ, MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    printf(1,"%d %x",*((int *)i));
    exit();
}
