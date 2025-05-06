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

int received = 0;
int exit_ready = 0;

void receiver(int sig){//pentru cand se primeste SIGUSR1 se va executa aceasta fucntie care seteaza var globala received pe 1
    received = 1;
}

char* reader(char *filename){
    int fd = open(filename,O_RDONLY);
    if(fd==-1)return NULL;

    char* buffer = malloc(256);

    size_t bytes = read(fd,buffer,255);
    if(bytes ==-1){
        free(buffer);
        close(fd);
        return NULL;
    }

    buffer[bytes] = '\0';//terminatorul de sir
    buffer[strcspn(buffer,"\n")] = '\0';
    close(fd);
    return buffer;
}

int log_entry(char * huntID, char * statement){
    char logPath[128];
    sprintf(logPath,"%s/%s", huntID, logfile);

    int fd = open(logPath, O_WRONLY|O_CREAT|O_APPEND, 0644);
    if(fd==-1) {
        perror("ERR: Deschidere fisier log esuata");
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
        printf("Hunt negasit sau nu exista comori in cadrul acestuia!\n");
        return -1;
    }

    printf("Hunt:%s\n", huntID);
    printf("Treasure file size: %ld B\n", st.st_size);

    char time_buff[64];
    strftime(time_buff,sizeof(time_buff), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));//momentul ultimei modificari

    printf("Momentul ultimei modificari: %s\n", time_buff);

    int fd = open(path,O_RDONLY);
    if(fd==-1){
        perror("ERR: Deschidere fisier comoara esuata");
        return -1;
    }

    Treasure tr;
    int contor = 0;

    printf("Comori:\n");
    printf("%-5s %-20s %-20s %-10s\n", "ID","User", "Coordinates", "Value");//aliniere header tabel

    printf("--------------------------------------------------------\n");

    while(read(fd,&tr,sizeof(Treasure))==sizeof(Treasure)){
        printf("%-5d %-15s (%-8.5f, %-8.5f) %10d\n",tr.ID,tr.username,tr.latitude,tr.longitude,tr.value);//aliniere tabel
        contor++;
    }

    if(contor==0){
        printf("Nicio comoara gasita in acest director hunt\n");
    }else{
        printf("Total comori: %d\n", contor);
    }

    close(fd);

    char statement[128];
    sprintf(statement, "S-au listat toate comorile din huntul cu ID %s [prin treasure_hub]", huntID);

    log_entry(huntID, statement);
    return 0;
}

int open_treasure_file(char *huntID, int flags){
    char filepath[128];
    sprintf(filepath, "%s/%s", huntID, treasurefile);

    int fd = open(filepath, flags, 0644);
    if(fd==-1) perror("ERR: Deschiderea fisierului de comori esuata");

    return fd;
}

int find_treasure(char* huntID, int treasureID, Treasure* tr){//pointer catre struct treasure ca sa stocam informatii in el
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
    if(find_treasure(huntID, treasureID, &tr)!=0){//informatia ajunge in adresa lui tr
        printf("Comoara cu ID %d nu a fost gasita in hunt-ul %s\n",treasureID, huntID);
        return -1;
    }

    printf("\nDetalii comoara:\n");
    printf("ID: %d\n", tr.ID);
    printf("Adaugata de utilizatorul: %s\n", tr.username);
    printf("Coordonate: %.5f, %.5f\n", tr.latitude, tr.longitude);
    printf("Valoare: %d\n", tr.value);
    printf("Indiciu: %s\n", tr.clue);

    //log entry
    char statement[256];
    sprintf(statement, "Comoara cu ID %d a fost vizualizata [prin treasure_hub]", treasureID);

    log_entry(huntID, statement);
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
        perror("ERR la OPENDIR in LIST_HUNTS");
        return;
    }    

    printf("###\tVANATORI\t###\n");

    struct dirent * hunt;
    struct stat st;

    while((hunt = readdir(huntdir))!=NULL){
        if(strcmp(hunt->d_name,".")==0 || strcmp(hunt->d_name,"..")==0
            || strcmp(hunt->d_name,".git")==0)continue;

        if(stat(hunt->d_name,&st)==0 && S_ISDIR(st.st_mode)){
            char filepath[512];
            sprintf(filepath,"%s/%s",hunt->d_name,treasurefile);
            int count = count_treasures(filepath);
            if(count>=0){
                printf("%s: %d\n", hunt->d_name, count);
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
        usleep(2000000);
    }else if(strcmp(command, "help")==0){
        printf("Comenzi:\nstart_monitor, list_hunts, list_treasures, view_treasure, stop_monitor\n");    
    }else{
        printf("Comanda necunoscuta: %s\n", command);
    }

    free(command);
    if(params){
        free(params);
    }

    usleep(200);
}

int main(){
    struct sigaction sa;
    sa.sa_handler = receiver;
    sa.sa_flags = 0;

    sigaction(SIGUSR1, &sa, NULL);

    while(!exit_ready){
        if(received){
            received = 0;
            process_command();
        }
        usleep(200000);
    }

    printf("Monitorul se stinge ... \n");
    return 0;
}