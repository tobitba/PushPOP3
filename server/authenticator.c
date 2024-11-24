
#include "../include/authenticator.h"
#include "../include/args.h"
#include <stdio.h>
#include <string.h>

static struct users* userList;
static int userCount;

void init_authenticator(struct users * newUsers, int newUserCount){
    userList = newUsers;
    userCount = newUserCount;
}


int userMatch(char* user, char* savedUser){
    return strcmp(user,savedUser) == 0;
}

int passMatch(char* pass, char* savedPass){
    return strcmp(pass,savedPass) == 0;
}

int isUserAndPassValid(char* user, char* pass){
    for(int i = 0; i < userCount; i++){
        if(userMatch(user,userList[i].pass) && passMatch(pass, userList[i].pass)){
            return 1;
        }
    }
    return 0;
}




