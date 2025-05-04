#include <stdio.h>
#include <sys/stat.h> 
#include <unistd.h> 
#include <fcntl.h> 
#include <time.h> 
#include <string.h> 
#include <sys/types.h>
#include <stdlib.h>
#include<dirent.h>

//definim pentru orice director de hunt, fisier treasure, va fi folosit mai jos
#define treasurefile "treasures"
//definim un fisier de log
#define logfile "logged_hunt"

typedef struct treasure{
    int ID;
    char username[20];
    double latitude;
    double longitude;
    char clue[200];
    int value;
}Treasure;

//functie pentru crearea de director, in cazul in care ele nu exista

int init_hunt_dir(char* huntID){
    struct stat st;

    if(stat(huntID, &st)==-1){
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
    
    unlink(symLink);
    
    if(symlink(logPath, symLink)==-1){
        perror("ERR: Crearea legaturii simbolice a esuat");
        return -1;
    }
    return 0;
}

int open_treasure_file(char *huntID, int flags){
    char filepath[128];
    sprintf(filepath, "%s/%s", huntID, treasurefile);

    int fd = open(filepath, flags, 0644);
    if(fd==-1) perror("ERR: Deschiderea fisierului de comori esuata");

    return fd;
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

//functie pentru a adauga o comoara
int add_treasure(char *huntID, struct treasure *newTreasure){
    if(init_hunt_dir(huntID)!=0) return -1;
    int fd = open_treasure_file(huntID, O_WRONLY|O_CREAT|O_APPEND);
    if(fd==-1) return -1;

    if(write(fd, newTreasure, sizeof(struct treasure))!=sizeof(struct treasure)){
        perror("ERR: Esuare la scrierea datelor despre comoara");
        close(fd);
        return -1; 
    }

    close(fd);
    
    char statement [256];
    sprintf(statement, "Comoara %d adaugata de catre utilizatorul %s",
    newTreasure->ID, newTreasure->username);

    log_entry(huntID, statement);

    creare_log_symlink(huntID);

    return 0;
}

int remove_treasure(char *huntID, int treasureID){
    char path[256];
    sprintf(path,"%s/temp", huntID);

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

    while(read(fd,&tr,sizeof(Treasure))==sizeof(Treasure)){
        if(tr.ID!=treasureID){ 
            if(write(temp,&tr,sizeof(Treasure))!=sizeof(Treasure)){
                perror("ERR: Nu s-a putut scrie in fisierul inlocuitor");
                close(fd);
                close(temp);
                unlink(path);
                return -1;
            }
        }else{
            found = 1;
        }
    }
    close(fd);
    close(temp);

    if(!found){
        unlink(path);
        return -1;
    }

    char filepath[128];
    sprintf(filepath, "%s/%s", huntID, treasurefile);

    if(rename(path,filepath)==-1){//facem inlocuirea
        perror("ERR: Nu s-a actualizat fisierul de comori");
        return -1; 
    }

    char statement[256];
    sprintf(statement, "Eliminat comoara cu ID %d", treasureID);
    log_entry(huntID, statement);
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
    printf("%-5s %-20s %-20s %-10s\n", "ID","User", "Coordinates", "Value");

    printf("--------------------------------------------------------\n");

    while(read(fd,&tr,sizeof(Treasure))==sizeof(Treasure)){
        printf("%-5d %-15s (%-8.5f, %-8.5f) %10d\n",tr.ID,tr.username,tr.latitude,tr.longitude,tr.value);
        contor++;
    }

    if(contor==0){
        printf("Nicio comoara gasita in acest director hunt\n");
    }else{
        printf("Total comori: %d\n", contor);
    }

    close(fd);

    char statement[64];
    sprintf(statement, "S-au listat toate comorile din huntul cu ID %s", huntID);

    log_entry(huntID, statement);

    return 0;
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

int view_treasure(char* huntID, int treasureID){
    Treasure tr;
    if(find_treasure(huntID, treasureID, &tr)!=0){
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
    sprintf(statement, "Comoara cu ID %d a fost vizualizata", treasureID);

    log_entry(huntID, statement);
    return 0;
}


int remove_hunt(char* huntID){
    char path[512];

    char logpath[128];
    sprintf(logpath,"%s-%s", logfile, huntID);
    unlink(logpath);

    struct stat st;
    if(stat(huntID,&st)!=0){
        perror("ERR: Stat");
        return -1;
    }

    DIR* dr = opendir(huntID);
    if(!dr){
        perror("ERR: opendir");
        return -1;
    }

    struct dirent * stdir;
    while((stdir=readdir(dr))){
        if(strcmp(stdir->d_name,".")==0||strcmp(stdir->d_name,"..")==0){
            continue;
        }

        sprintf(path,"%s/%s", huntID, stdir->d_name);
        if(remove(path)!=0){
            perror("ERR: REMOVE");
            return -1;
        }
    }

    closedir(dr);

    if(rmdir(huntID)==0){
        printf("Directorul a fost sters cu SUCCES\n");
    }else{
        printf("Directorul de hunt nu a fost sters\n");
        return -1;
    }

    return 0;
}

void instructiuni(){
    printf("FOLOSIRE: Prin adaugarea urmatoarelor stringuri ca argumente in linie comanda;\n");
    printf("\t\"add <huntID>\", unde huntID e string\n");
    printf("\t\"list <huntID>\"\n");
    printf("\t\"view <huntID> <treasureID>\", unde treasureID este un intreg\n");
    printf("\t\"remove_treasure <huntID> <treasureID>\"\n");
    printf("\t\"remove_hunt <huntID>\"\n");
    printf("exemplu rulare: ./treasure_manager add Hunt1\n");
}

int main(int argc, char** argv){
    if(argc==1 || strcmp(argv[1],"help")==0){
        instructiuni();
        return 1;
    }
    if(strcmp(argv[1],"add")==0){
        if(argc==2){
            printf("huntID lipsa\n");
            return 1;
        }

        char* huntID = argv[2];

        Treasure tr;
        printf("ID comoara: ");
        scanf("%d", &tr.ID);

        printf("Username pana la 20 de caractere: ");
        scanf("%s", tr.username);
        
        printf("Coordonate GPS (sub forma [x,y]): ");
        scanf("%lf,%lf", &tr.latitude, &tr.longitude);
        
        printf("Indiciu, max 200 de caractere: ");
        getchar();
        fgets(tr.clue, 200, stdin);
        
        printf("Valoare: ");
        scanf("%d", &tr.value);

        if(add_treasure(huntID, &tr)==0){
            printf("Comoara adaugata cu succes\n");
        }else{
            printf("Nu s-a adaugat comoara\n");
            return 1;
        }
    }else if(strcmp(argv[1],"list")==0){
        if(argc<3){
            printf("huntID lipsa\n");
            return 1;
        }

        list_treasures(argv[2]);
    }else if(strcmp(argv[1],"view")==0){
        if(argc<4){
            printf("huntID sau treasureID lipsa\n");
            return 1;
        }
        view_treasure(argv[2],atoi(argv[3]));
    }else if(strcmp(argv[1],"remove_hunt")==0){
        if(argc<3){
            printf("huntID lipsa\n");
            return 1;
        }

        remove_hunt(argv[2]);
    }else if(strcmp(argv[1],"remove_treasure")==0){
        if(argc<4){
            printf("huntID sau treasureID lipsa");
            return 1;
        }
        if(remove_treasure(argv[2],atoi(argv[3]))==0){
            printf("Comoara stearsa cu succes\n");
        }else{
            printf("Comoara nu a fost stearsa/gasita!\n");
            return 1;
        }
    }else{
        printf("Comanda necunoscuta: %s\n", argv[1]);
        printf("Pentru instructiuni apelati programul fara argumente in plus sau folositi argumentul \"help\"\n");
        return 1;
    }
    return 0;
}
