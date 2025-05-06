#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>


//voi folosi fisiere intermediare pt a comunica intre hub si copil(treasure_monitor)
#define CMD_FILE "command.txt"
//pentru parametrii comenzilor(daca e cazul, parametrii sunt in felul urmator: pt view_treasures este nevoie de huntID)
#define PARAM_FILE "params.txt"

pid_t monitor_pid = -1;
int monitor_running = 0;
int termination = 0;

void exit_monitor(int sig){
    int status;
    pid_t pid;

    while((pid = wait(&status)) > 0){//se asteapta sa "moara" procesul copil
        if (pid == monitor_pid) {//daca moare:
            if(WIFEXITED(status)){//iesire normala
                printf("S-a iesit din procesul monitor cu status: %d\n", WEXITSTATUS(status));
            }else if(WIFSIGNALED(status)){//eliminare de la un semnal
                printf("Procesul monitor a fost terminat cu semnal: %d\n", WTERMSIG(status));
            }
            monitor_pid = -1;
            monitor_running = 0;
            termination = 0;
        }
    }
    usleep(200000);
}

void write_command(char* command){
    int fd = open(CMD_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(fd==-1){
        perror("OPEN FUNCTION FAIL CMD");
        return;
    }
    write(fd,command,strlen(command));
    close(fd);
}

void write_params(char* params){
    int fd = open(PARAM_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(fd==-1){
        perror("OPEN FUNCTION FAIL PARAM");
    }
    write(fd,params,strlen(params));
    close(fd);
}

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

void send_command(char* command, char* params){
    if(!monitor_running){
        printf("Monitorul nu ruleaza\n");
        return;
    }

    if(termination){
        printf("Monitorul se opreste\n");
        return;
    }

    write_command(command);
    if(params!=NULL){
        write_params(params);
    }

    if(kill(monitor_pid,SIGUSR1)==-1){
        perror("Eroare trimitere SIGUSR1 la send_command");
    }
}

void stop_monitor(){
    if(!monitor_running){
        printf("Monitorul nu ruleaza\n");
        return;
    }

    if(termination){
        printf("Monitorul deja e in procesul de oprire\n");
        return;
    }

    write_command("stop");
    if(kill(monitor_pid,SIGUSR1)==-1){
        perror("Eroare trimitere SIGUSR1 la stop_monitor");
    }else{
        termination = 1;
        printf("Se asteapta oprirea monitorului\n");
    }
}

int main(){
    char command[32];
    char huntID[128];
    char treasureID[128];
    char params[512];

    struct sigaction sa;
    sa.sa_handler = exit_monitor;
    sa.sa_flags = SA_RESTART;

    sigaction(SIGCHLD,&sa,NULL);

    printf("||\tTREASURE HUB\t||\n");
    printf("Comenzi: start_monitor, list_hunts, list_treasures, view_treasure, stop_monitor, exit\n");

    while(1){
        usleep(20001);
        printf("\ntreasure_hub> ");
        if(fgets(command, sizeof(command), stdin) == NULL){
            break;
        }
        command[strcspn(command, "\n")] = 0; //strcspn pt a elimina newline

        if(strcmp(command, "start_monitor") == 0){
            start_monitor();
        }else if(strcmp(command, "list_hunts") == 0){
            send_command("list_hunts", NULL);
        }else if(strcmp(command, "list_treasures") == 0){
            printf("Introduceti huntID: ");
            if(fgets(huntID, sizeof(huntID), stdin)!=NULL){
                huntID[strcspn(huntID, "\n")] = '\0';
                send_command("list_treasures", huntID);
            }
        }else if(strcmp(command, "view_treasure")==0){
            printf("Enter hunt ID: ");
            if(fgets(huntID, sizeof(huntID), stdin) != NULL){
                huntID[strcspn(huntID, "\n")] = '\0';
                printf("Introduceti treasureID(numar intreg): ");
                if(fgets(treasureID, sizeof(treasureID), stdin) != NULL){
                    treasureID[strcspn(treasureID, "\n")] = '\0';
                    sprintf(params, "%s %s", huntID, treasureID);
                    send_command("view_treasure", params);
                }
            }
        }else if(strcmp(command,"stop_monitor") == 0){
            stop_monitor();
            usleep(200000);
        }else if(strcmp(command, "exit") == 0){
            if(monitor_running){
                printf("ERR: monitorul inca ruleaza\n");
            }else{
                printf("Proces incheiat\n");
                break;
            }
        }else if(strcmp(command, "help")==0){
            send_command("help", NULL);
        }else{
            printf("Comanda necunoscuta: %s\n", command);
        }
    }

    return 0;
}
