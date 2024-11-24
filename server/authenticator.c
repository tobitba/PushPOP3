
#include "../include/authenticator.h"
#include "../include/args.h"
#include <stdio.h>

static struct users* userList;

void init_authenticator(struct users * newUsers){
    userList = newUsers;
}





