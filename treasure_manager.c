#include <stdio.h>
#include <sys/stat.h> //pt stat, lstat, mkdir etc.
#include <unistd.h> //pt symlink, unlink etc
#include <fcntl.h> //pt open, write, read, close, flags etc.
#include <time.h> //pentru toate functiile asociate cu timpul si data, folosite la log entries
#include <string.h> //pt operatii cu stringuri

//definim pentru orice director de hunt, fisier treasure, va fi folosit mai jos
#define treasurefile "treasures"
//definim un fisier de log
#define logfile "huntLogger"

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

    sprintf(logPath, "%s/%s", huntID, logfile);//memoram in formatul adecvat calea catre hunt

    sprintf(symLink, "%s-%s", logfile, huntID);//acelasi lucru dar pentru legatura simbolica
    
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
int open_treasure_file(char *huntID, int flags){
    char filepath[128];
    sprintf(filepath, "%s/%s", huntID, treasurefile);

    int fd = open(filepath, flags, 0644);
    if(fd==-1) perror("ERR: Deschiderea fisierului de comori esuata");

    return fd;
}

//functia de scriere de log entries, pentru a tine cont de actiunile facute
int log_entry(char * huntID, char * statement){
    char logPath[128];
    sprintf(logPath,"%s/%s", huntID, logfile);//obtinem filepathul pt log

    //deschidem fisierul de log cu flagurile: write only, create, append
    int fd = open(logPath, O_WRONLY|O_CREAT|O_APPEND, 0644);
    if(fd==-1) {
        perror("ERR: Deschidere fisier log esuata");
        return -1;//fail
    }

    /*variabile time_t cu ajutorul librariei <time.h> pentru 
    a fi folosita in functia localtime(time_t*)*/
    time_t present = time(NULL);//obtinem timpul in secunde de la Epoch, 1970-01-01 
    // conform paginii de MANUAL (man 2 time)
    char timp[64];
    // pentru afisarea momentului actiunii am gasit functia strftime
    strftime(timp, sizeof(timp), "%Y.%m.%d %H:%M:%S", localtime(&present));/*localtime returneaza un 
     pointer de struct tm(broken down time, adica an, luna zi... toate separate
    intr-un struct interpretabil)*/

    //scriem entry-ul efectiv
    char entry[256];
    sprintf(entry, "Moment: %s; %s\n", timp, statement);

    write(fd, entry, strlen(entry));
    close(fd);//inchidem fisierul
    return 0;//succes
}

//functie pentru a adauga o comoara
int add_treasure(char *huntID, struct treasure *newTreasure){
    //pentru usurinta, initializam un hunt directory dupa id-ul dat ca parametru daca acesta nu exista
    if(init_hunt_dir(huntID)!=0) return -1;//fail

    //deschidem cu flagul de creare si append (si write only) un fisier treasure
    int fd = open_treasure_file(huntID, O_WRONLY|O_CREAT|O_APPEND);
    if(fd==-1) return -1;//fail

    //scriem datele din structul nostru in fisier
    if(write(fd, newTreasure, sizeof(struct treasure))!=sizeof(struct treasure)){
        perror("ERR: Esuare la scrierea datelor despre comoara");
        close(fd);//se inchide fisierul
        return -1; //fail
    }

    close(fd);

    //scriem log entry
    char statement [256];
    sprintf(statement, "Treasure %d added by user %s",
    newTreasure->ID, newTreasure->username);

    log_entry(huntID, statement);

    creare_log_symlink(huntID);

    return 0;
}

int main(){
    init_hunt_dir("Hunt1");
    creare_log_symlink("Hunt1");
    open_treasure_file("Hunt1", O_CREAT|O_APPEND);
    struct treasure tr1;
    tr1.ID = 10;
    add_treasure("Hunt1", &tr1);
    return 0;
}