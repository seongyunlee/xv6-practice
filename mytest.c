#include "types.h"
#include "user.h"
#include "stat.h"

int main(){
    printf(1,"test program start\n");
    int c_pid=fork();
    printf(1,"Process(:%d) for test is on running\n",getpid());
    
    if(c_pid==0){
        setnice(getpid(),39);
    }
    else{
        setnice(getpid(),0);
    }
    int cnt=0;
    int j=0;
    ps(0);
    while(j<100){
        int i = 0;
        while(i<100000000){
            if(i%13434==0)
                cnt++;
            i++;
        }
        if(c_pid==0 && j==50){
            printf(1,"%d sleeps\n",getpid());
            sleep(100);
            printf(1,"%d is waked up\n",getpid());
        }
        printf(1,"%d : %d/100\n",getpid(),j);
        if(j%10==0)
            ps(0);
        j++;
    }
    printf(1,"%d\n",cnt);
    ps(0);
    wait();
    printf(1,"Process(:%d) exit\n",getpid());
    exit();
}
