#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>

//voi folosi fisiere intermediare pt a comunica intre hub si copil(treasure_monitor)
#define CMD_FILE "command.txt"
//pentru parametrii comenzilor(daca e cazul, parametrii sunt in felul urmator: pt view_treasures este nevoie de huntID)
#define PARAM_FILE "params.txt"

pid_t monitor_pid = -1;
int monitor_running = 0;
int termination = 0;

int pfd[2];

void exit_monitor(int sig){
    int status;
    pid_t pid;

    while((pid = wait(&status)) > 0){//se asteapta sa "moara" procesul copil
        if (pid == monitor_pid) {//daca moare:
            printf("Proces monitor oprit cu status %d\n", status);
            monitor_pid = -1;
            monitor_running = 0;
            termination = 0;
        }
    }
    usleep(200);
}

void write_command(char* command){
    int fd = open(CMD_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(fd == -1){
        perror("OPEN FUNCTION FAIL CMD");
        return;
    }
    write(fd, command, strlen(command));
    close(fd);
}

void write_params(char* params){
    int fd = open(PARAM_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(fd == -1){
        perror("OPEN FUNCTION FAIL PARAM");
    }
    write(fd, params, strlen(params));
    close(fd);
}

void start_monitor(){
    if(monitor_running){
        printf("Monitorul ruleaza deja!\n");
        return;
    }

    if(pipe(pfd) == -1){
        perror("ERR AT PIPE");
        return;
    }

    pid_t pid = fork();

    if(pid == -1){
        perror("FAIL FORK");
        return;
    }else if(pid == 0){
        close(pfd[0]);
        
        char write_end[10];
        sprintf(write_end, "%d", pfd[1]);
        
        execl("./monitor", "monitor", write_end, NULL);
        perror("FAIL EXECL");
        exit(-1);
    }else{
        close(pfd[1]); 
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
    if(params != NULL){
        write_params(params);
    }

    if(kill(monitor_pid, SIGUSR1) == -1){
        perror("Eroare trimitere SIGUSR1 la send_command");
    }
}

void read_monitor_output(){
    char buffer[2048];
    int bytes;
    
    while((bytes = read(pfd[0], buffer, sizeof(buffer) - 1)) > 0){
        buffer[bytes] = '\0';
        printf("%s", buffer);
        fflush(stdout);
        if(bytes < 2047) break;
    }
}

void calculate_score(){
    DIR* dir = opendir(".");
    if(!dir) return;
    struct dirent* entry;
    struct stat st;

    while(monitor_running){
        usleep(1000000);
    }
    
    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        if(stat(entry->d_name, &st) == 0 && S_ISDIR(st.st_mode)){
            char path[512];
            snprintf(path, sizeof(path), "%s/treasures", entry->d_name);
            int fd = open(path, O_RDONLY);
        
            if(fd == -1) continue;
            close(fd);
        
            int pipefd[2];
        
            if(pipe(pipefd) == -1) continue;
        
            pid_t pid = fork();
            if(pid == -1){
                close(pipefd[0]);
                close(pipefd[1]);
                continue;
            }

            if(pid == 0){
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                execl("./calculate_score", "calculate_score", path, NULL);
                exit(1);
            }else{
                close(pipefd[1]);
                char buffer[256];
                int bytes;
                printf("Score pentru hunt %s:\n", entry->d_name);
                while((bytes = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0){
                    buffer[bytes] = '\0';
                    printf("%s", buffer);
                }
                close(pipefd[0]);
                waitpid(pid, NULL, 0);
                printf("\n");
            }
        }
    }
    closedir(dir);
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
    if(kill(monitor_pid, SIGUSR1) == -1){
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

    sigaction(SIGCHLD, &sa, NULL);

    printf("||\tTREASURE HUB\t||\n");
    printf("Comenzi: start_monitor, list_hunts, list_treasures, view_treasure, calculate_score, stop_monitor, exit\n");

    while(1){
        usleep(2000);
        printf("\ntreasure_hub> ");
        
        if(fgets(command, sizeof(command), stdin) == NULL){
            break;
        }
        command[strcspn(command, "\n")] = 0; //strcspn pt a elimina newline

        if(strcmp(command, "start_monitor") == 0){
            start_monitor();
        }else if(strcmp(command, "list_hunts") == 0){
            send_command("list_hunts", NULL);
            read_monitor_output();
        }else if(strcmp(command, "list_treasures") == 0){
            printf("Introduceti huntID: ");
            if(fgets(huntID, sizeof(huntID), stdin) != NULL){
                huntID[strcspn(huntID, "\n")] = '\0';
                send_command("list_treasures", huntID);
                read_monitor_output();
            }
        }else if(strcmp(command, "view_treasure") == 0){
            printf("Enter hunt ID: ");
            if(fgets(huntID, sizeof(huntID), stdin) != NULL){
                huntID[strcspn(huntID, "\n")] = '\0';
                printf("Introduceti treasureID(numar intreg): ");
                if(fgets(treasureID, sizeof(treasureID), stdin) != NULL){
                    treasureID[strcspn(treasureID, "\n")] = '\0';
                    sprintf(params, "%s %s", huntID, treasureID);
                    send_command("view_treasure", params);
                    read_monitor_output();
                }
            }
        }else if(strcmp(command, "calculate_score") == 0){
            stop_monitor();
            sleep(3);
            printf("\n");
            calculate_score();
        }
        else if(strcmp(command, "stop_monitor") == 0){
            stop_monitor();
        }else if(strcmp(command, "exit") == 0){
            if(monitor_running){
                printf("ERR: monitorul inca ruleaza\n");
            }else{
                printf("Proces incheiat\n");
                break;
            }
        }else if(strcmp(command, "help") == 0){
            send_command("help", NULL);
            read_monitor_output();
        }else{
            printf("Comanda necunoscuta: %s\n", command);
        }
    }

    return 0;
}
