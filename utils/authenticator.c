#include "../include/authenticator.h"
#include "../include/args.h"
#include <stdbool.h>
#include <string.h>

static User* userList;
static int userCount;

void initAuthenticator(User* newUsers, int newUserCount) {
    userList = newUsers;
    userCount = newUserCount;
}

bool userMatch(char* user, char* savedUser) {
    return strcmp(user, savedUser) == 0;
}

bool passMatch(char* pass, char* savedPass) {
    return strcmp(pass, savedPass) == 0;
}

int isUserAndPassValid(char* user, char* pass) {
    for (int i = 0; i < userCount; i++) {
        if (userMatch(user, userList[i].name) && passMatch(pass, userList[i].pass)) {
            return 1;
        }
    }
    return 0;
}
