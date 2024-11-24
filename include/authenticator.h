#ifndef AUTHENTICATOR
#define AUTHENTICATOR
#include "../include/args.h"

void init_authenticator(struct users* userSet, int newUserCount);

/**
 * @param user : the username to validate
 * @param pass : pass to validate
 * @return 1 if user and pass is valid, else 0
 */
int isUserAndPassValid(char* user, char* pass);


#endif