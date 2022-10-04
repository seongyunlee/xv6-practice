#include "types.h"
#include "user.h"
#include "stat.h"

int main(){
    int i;
    
    printf(1,"test pid %d\n",getpid());

    //test getpname
    for (i=1;i<11;i++){
        printf(1,"%d: ",i);
        if (getpname(i))
            printf(1,"Wrong pid\n");
    }
    //test getnice
    for(i=1;i<11;i++){
        printf(1,"%d nice : %d\n",i,getnice(i));
    }
    
    //test ps
    ps(0);

    //test setnice
    for(i=1;i<11;i++){
        setnice(i,i+5);
    }

    //test ps
    ps(0);
    for(i=0;i<20;i++){
        printf(1,"ps test %d\n",i);
        ps(i);
    }
    

    exit();
}
