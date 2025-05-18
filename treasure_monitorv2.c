#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <stdarg.h>

#define CMD_FILE "command.txt"
#define PARAM_FILE "params.txt"
#define treasurefile "treasures"
#define logfile "logged_hunt"

typedef struct treasure{
    int ID; 
    char username[20];
    double latitude;
    double longitude;
    char clue[200];
    int value;
}Treasure;

int pfd = -1;
int received = 0;
int exit_ready = 0;

void send_to_hub(const char* msg) {
    if (pfd != -1) {
        write(pfd, msg, strlen(msg));
    }
}

void receiver(int sig){
    received = 1;
}

char* reader(char *filename){
    int fd = open(filename,O_RDONLY);
    if(fd==-1) return NULL;

    char* buffer = malloc(256);

    size_t bytes = read(fd,buffer,255);
    if(bytes ==-1){
        free(buffer);
        close(fd);
        return NULL;
    }

    buffer[bytes] = '\0';
    buffer[strcspn(buffer,"\n")] = '\0';
    close(fd);
    return buffer;
}

int log_entry(char * huntID, char * statement){
    char logPath[128];
    sprintf(logPath,"%s/%s", huntID, logfile);

    int fd = open(logPath, O_WRONLY|O_CREAT|O_APPEND, 0644);
    if(fd==-1) {
        char buffr[128];
        sprintf(buffr, "ERR: Deschidere fisier log esuata\n");
        send_to_hub(buffr);
        return -1;
    }

    time_t present = time(NULL);
    char timp[64];
    strftime(timp, sizeof(timp), "%Y.%m.%d %H:%M:%S", localtime(&present));
    
    char entry[256];
    sprintf(entry, "Moment: %s; %s\n", timp, statement);

    write(fd, entry, strlen(entry));
    close(fd);
    return 0;
}


int list_treasures(char * huntID){
    char path[256];
    sprintf(path, "%s/%s", huntID, treasurefile);

    struct stat st;

    if(stat(path,&st) == -1){
        char buffr[128];
        sprintf(buffr, "Hunt negasit sau nu exista comori in cadrul acestuia!\n");
        send_to_hub(buffr);
        return -1;
    }

    char buffr[256];
    sprintf(buffr, "Hunt:%s\n", huntID);
    send_to_hub(buffr);

    sprintf(buffr, "Treasure file size: %ld B\n", st.st_size);
    send_to_hub(buffr);

    char time_buff[64];
    strftime(time_buff,sizeof(time_buff), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));

    sprintf(buffr, "Momentul ultimei modificari: %s\n", time_buff);
    send_to_hub(buffr);

    int fd = open(path,O_RDONLY);
    if(fd==-1){
        char buffr[128];
        sprintf(buffr, "ERR: Deschidere fisier comoara esuata\n");
        send_to_hub(buffr);
        return -1;
    }

    Treasure tr;
    int contor = 0;

    sprintf(buffr, "Comori:\n");
    send_to_hub(buffr);

    sprintf(buffr, "%-5s %-20s %-20s %-10s\n", "ID","User", "Coordinates", "Value");
    send_to_hub(buffr);

    sprintf(buffr, "--------------------------------------------------------\n");
    send_to_hub(buffr);

    while(read(fd,&tr,sizeof(Treasure))==sizeof(Treasure)){
        sprintf(buffr, "%-5d %-15s (%-8.5f, %-8.5f) %10d\n",tr.ID,tr.username,tr.latitude,tr.longitude,tr.value);
        send_to_hub(buffr);
        contor++;
    }

    if(contor==0){
        sprintf(buffr, "Nicio comoara gasita in acest director hunt\n");
        send_to_hub(buffr);
    }else{
        sprintf(buffr, "Total comori: %d\n", contor);
        send_to_hub(buffr);
    }

    close(fd);

    sprintf(buffr, "S-au listat toate comorile din huntul cu ID %s [prin treasure_hub]", huntID);
    log_entry(huntID, buffr);
    return 0;
}

int open_treasure_file(char *huntID, int flags){
    char filepath[128];
    sprintf(filepath, "%s/%s", huntID, treasurefile);

    int fd = open(filepath, flags, 0644);
    if(fd==-1) {
        char buffr[128];
        sprintf(buffr, "ERR: Deschiderea fisierului de comori esuata\n");
        send_to_hub(buffr);
    }

    return fd;
}

int find_treasure(char* huntID, int treasureID, Treasure* tr){
    int fd = open_treasure_file(huntID, O_RDONLY);
    if(fd==-1) return -1;
    int found = 0;

    while(read(fd,tr,sizeof(Treasure))==sizeof(Treasure)){
        if (treasureID == tr->ID){
            found = 1;
            break;
        }
    }

    close(fd);
    return found ? 0:-1;

}

int view_treasure(char* params){
    char* huntID = strtok(params, " ");
    char* treasureID_string = strtok(NULL, " ");
    int treasureID = atoi(treasureID_string);
    Treasure tr;
    char buffr[256];

    if(find_treasure(huntID, treasureID, &tr)!=0){
        sprintf(buffr, "Comoara cu ID %d nu a fost gasita in hunt-ul %s\n",treasureID, huntID);
        send_to_hub(buffr);
        return -1;
    }

    sprintf(buffr, "\nDetalii comoara:\n");
    send_to_hub(buffr);

    sprintf(buffr, "ID: %d\n", tr.ID);
    send_to_hub(buffr);

    sprintf(buffr, "Adaugata de utilizatorul: %s\n", tr.username);
    send_to_hub(buffr);

    sprintf(buffr, "Coordonate: %.5f, %.5f\n", tr.latitude, tr.longitude);
    send_to_hub(buffr);

    sprintf(buffr, "Valoare: %d\n", tr.value);
    send_to_hub(buffr);

    sprintf(buffr, "Indiciu: %s\n", tr.clue);
    send_to_hub(buffr);

    sprintf(buffr, "Comoara cu ID %d a fost vizualizata [prin treasure_hub]", treasureID);
    log_entry(huntID, buffr);

    return 0;
}

int count_treasures(char* filepath){
    int fd = open(filepath,O_RDONLY);
    if(fd<0) return -1;

    Treasure tr;
    int count = 0;

    while((read(fd,&tr,sizeof(Treasure)))==sizeof(Treasure)){
        count++;
    }

    close(fd);
    return count;
}

void list_hunts(){
    DIR* huntdir = opendir(".");
    if(!huntdir){
        char buffr[128];
        sprintf(buffr, "ERR la OPENDIR in LIST_HUNTS\n");
        send_to_hub(buffr);
        return;
    }    

    //char buffr[1028];
    printf("###\tVANATORI\t###\n");
    //send_to_hub(buffr);

    struct dirent * hunt;
    struct stat st;

    while((hunt = readdir(huntdir))!=NULL){
        if(strcmp(hunt->d_name,".")==0 || strcmp(hunt->d_name,"..")==0
            || strcmp(hunt->d_name,".git")==0) continue;

        if(stat(hunt->d_name,&st)==0 && S_ISDIR(st.st_mode)){
            char filepath[512];
            sprintf(filepath,"%s/%s",hunt->d_name,treasurefile);
            int count = count_treasures(filepath);
            if(count>=0){
                printf("%s: %d comori\n", hunt->d_name, count);
                //send_to_hub(buffr);
            }
        }
    }
    closedir(huntdir);
}

void process_command(){
    char* command = reader(CMD_FILE);
    if(!command){
        return;
    }

    char* params = reader(PARAM_FILE);

    if(strcmp(command, "list_hunts") == 0){
        list_hunts();
    }else if(strcmp(command, "list_treasures") == 0 && params){
        list_treasures(params);
    }else if(strcmp(command, "view_treasure") == 0 && params){
        view_treasure(params);
    }else if(strcmp(command, "stop") == 0){
        exit_ready = 1;
        //usleep(2000000);
    }else if(strcmp(command, "help")==0){
        send_to_hub("Comenzi:\nstart_monitor, list_hunts, list_treasures, view_treasure, calculate_score, stop_monitor, exit\n");
    }else{
        char buffr[128];
        sprintf(buffr, "Comanda necunoscuta: %s\n", command);
        send_to_hub(buffr);
    }

    free(command);
    if(params){
        free(params);
    }

    usleep(2000000);
}

int main(int argc, char** argv){
    if(argc<2){
        char buffr[128];
        sprintf(buffr, "ERR AT ARGUMENTS FOR MONITOR\n");
        send_to_hub(buffr);
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = receiver;
    sa.sa_flags = 0;

    sigaction(SIGUSR1, &sa, NULL);

    pfd = atoi(argv[1]);

    while(!exit_ready){
        if(received){
            received = 0;
            process_command();
        }
        usleep(20000);
    }

    send_to_hub("Monitorul se stinge ... \n");
    return 0;
}
