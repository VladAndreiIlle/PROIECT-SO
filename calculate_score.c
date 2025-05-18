#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct treasure{
    int ID;
    char username[20];
    double latitude;
    double longitude;
    char clue[200];
    int value;
}Treasure;

typedef struct user_score{
    char username[20];
    int total;
}UserScore;

//int pfd = -1;

#define MAX_USERS 100

int main(int argc, char *argv[]){
    if(argc != 2){
        perror("ERR AT ARGUMENTS IN CALC SCORES");
        return 1;
    }

    //pfd = atoi(argv[2]);
    //char buffr[512];

    int fd = open(argv[1], O_RDONLY);
    if(fd == -1){
        perror("ERR AT OPEN IN CALC SCORES");
        return 1;
    }

    UserScore scores[MAX_USERS];
    int user_count = 0;

    Treasure tr;
    while(read(fd, &tr, sizeof(Treasure)) == sizeof(Treasure)){
        int found = 0;
        for(int i = 0; i < user_count; i++){
            if(strcmp(scores[i].username, tr.username) == 0){
                scores[i].total += tr.value;
                found = 1;
                break;
            }
        }
        if(!found && user_count < MAX_USERS){
            strcpy(scores[user_count].username, tr.username);
            scores[user_count].total = tr.value;
            user_count++;
        }
    }
    close(fd);

    for(int i = 0; i < user_count; i++){
        printf("%s: %d\n", scores[i].username, scores[i].total);
    }

    return 0;
}
