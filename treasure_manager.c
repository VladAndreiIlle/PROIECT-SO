#include <stdio.h>
#include <sys/stat.h> //pt stat, lstat, mkdir etc.
#include <unistd.h> //pt symlink, unlink etc
#include <fcntl.h> //pt open, write, read, close, flags etc.

struct treasure{
    int ID; //am ales tip de date int pentru a facilita simplitate si a fi mai usor de cautat
    char username[20];
    double latitude;
    double longitude;
    char clue[200];
    int value;
};

//functie pentru crearea de director, in cazul in care ele nu exista

int init_hunt_dir(char* huntID){//am ales tip de return int pt a putea verifica ce s-a intamplat in functie
    //voi aplica acest principiu in mai multe locuri
    struct stat st;

    if(stat(huntID, &st)==-1){//daca are loc egalitatea cu -1, directorul nu exista, asa ca se creaza
        if(mkdir(huntID, 0777)==-1){
            perror("ERR: Eroare la crearea de director");
            return -1;//cazul de esuare
        }
        printf("S-a creat noul director: %s\n", huntID);
    }

    return 0;//cazul de succes
}

int creare_log_symlink(char* huntID){
    char logPath[128];
    char symLink[128];

    sprintf(logPath, "%s/%s", huntID, "huntLogger");//memoram in formatul adecvat calea catre hunt

    sprintf(symLink, "%s-%s", "huntLogger", huntID);//acelasi lucru dar pentru legatura simbolica
    
    //daca deja exista, vom dezbina legatura:
    unlink(symLink);

    // dupa aceasta cream legatura
    if(symlink(logPath, symLink)==-1){
        perror("ERR: Crearea legaturii simbolice a esuat");
        return -1;
    }
    return 0;
}

//functie pentru a deschide un fisier treasure
int open_treasure_file(char *huntID, char* treasureFileName, int flags){
    char filepath[128];
    sprintf(filepath, "%s/%s", huntID, treasureFileName);

    int fd = open(filepath, flags, 0644);
    if(fd==-1) perror("ERR: Deschiderea fisierului de comori esuata");

    return fd;
}


int main(){
    init_hunt_dir("Hunt1");
    creare_log_symlink("Hunt1");
    open_treasure_file("Hunt1", "TreasureFile1.txt", O_CREAT|O_APPEND);
    return 0;
}