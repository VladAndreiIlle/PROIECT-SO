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

typedef struct treasure{
    int ID; //am ales tip de date int pentru a facilita simplitate si a fi mai usor de cautat
    char username[20];
    double latitude;
    double longitude;
    char clue[200];
    int value;
}Treasure;

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

//functie pentru a sterge comori dupa ID
int remove_treasure(char *huntID, int treasureID){
    char path[256];
    sprintf(path,"%s/temp", huntID);//facem o cale (inlocuitoare)

    int fd = open_treasure_file(huntID, O_RDONLY);
    if(fd==-1) return -1;//fail

    int temp = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(temp==-1){
        close(fd);
        perror("ERR: Nu s-a creat fisierul temporar(pentru inlocuirea la operatia remove)");
        return -1; //fail
    }

    Treasure tr;
    int found = 0;

    //citim din fd original si scriem in temp toate comorile inafara de cea pentru stergere
    while(read(fd,&tr,sizeof(Treasure))==sizeof(Treasure)){//se citeste cate un struct treasure !!
        if(tr.ID!=treasureID){//doar daca nu e cel precizat in parametru se scrie in fisierul inlocuitor.
            if(write(temp,&tr,sizeof(Treasure))!=sizeof(Treasure)){
                perror("ERR: Nu s-a putut scrie in fisierul inlocuitor");
                close(fd);
                close(temp);
                unlink(path);// stergere fisier temporar din file system
                return -1;
            }
        }else{
            found = 1;
        }
    }
    close(fd);
    close(temp);

    if(!found){
        unlink(path);//stergere fisier temporar din file system, de data asta la cazul in care nu e gasit
        return -1;
    }

    char filepath[128];
    sprintf(filepath, "%s/%s", huntID, treasurefile);

    if(rename(path,filepath)==-1){//facem inlocuirea
        perror("ERR: Nu s-a actualizat fisierul de comori");
        return -1; //fail
    }

    char statement[256];
    sprintf(statement, "Removed treasure %d", treasureID);
    log_entry(huntID, statement);
    return 0;
}

int list_treasures(char * huntID){
    char path[256];
    sprintf(path, "%s/%s", huntID, treasurefile);

    struct stat st;

    if(stat(path,&st) == -1){
        printf("Hunt not found or treasures not added yet!\n");
        return -1;
    }

    printf("Hunt:%s\n", huntID);
    printf("TReasure file size: %ld B\n", st.st_size);

    char time_buff[64];
    strftime(time_buff,sizeof(time_buff), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));//momentul ultimei modificari

    printf("Last modification took place at: %s\n", time_buff);

    int fd = open(path,O_RDONLY);
    if(fd==-1){
        perror("ERR: Deschidere fisier comoara esuata");
        return -1;
    }

    Treasure tr;
    int contor = 0;

    printf("Treasures:\n");
    printf("%-5s %-20s %-20s %-10s\n", "ID","User", "Coordinates", "Value");//aliniere header tabel

    printf("--------------------------------------------------------\n");

    while(read(fd,&tr,sizeof(Treasure))==sizeof(Treasure)){
        printf("%-5d %20s (%-5.6f, %-5.6f) %-10d\n",tr.ID,tr.username,tr.latitude,tr.longitude,tr.value);//aliniere tabel
        contor++;
    }

    if(contor==0){
        printf("Nicio comoara gasita in acest director hunt\n");
    }else{
        printf("Total comori: %d\n", contor);
    }

    close(fd);

    char statement[64];
    sprintf(statement, "Listed all treasures from hunt %s", huntID);

    log_entry(huntID, statement);

    return 0;
}

/*void instructiuni(){
    printf("FOLOSIRE: ")
}*/

int main(){
    init_hunt_dir("Hunt1");
    creare_log_symlink("Hunt1");
    open_treasure_file("Hunt1", O_CREAT|O_APPEND);
    struct treasure tr1;
    tr1.ID = 10;
    add_treasure("Hunt1", &tr1);
    //remove_treasure("Hunt1", 10);
    list_treasures("Hunt1");
    return 0;
}