#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pid_t monitor_pid = -1;
int monitor_running = 0;

void start_monitor(){
    if(monitor_running){
        printf("Monitorul ruleaza deja!\n");
        return;
    }

    pid_t pid = fork();

    if(pid == -1){
        perror("FAIL FORK");
        return;
    }else if(pid==0){
        execl("./monitor", "monitor", NULL);
        perror("FAIL EXECL");
        exit(-1);
    }else{
        monitor_pid = pid;
        monitor_running = 1;
        printf("Child process initializat cu pid: %d\n", monitor_pid);
    }


}

int main(){
    start_monitor();
    return 0;
}