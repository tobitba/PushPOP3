#include "../include/pop3.h"
#include "../include/buffer.h"
#include "../include/commands.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#define MAX_COMMAND_LENGHT 5
#define MAX_ARG_LENGHT 41
#define COMMAND_COUNT 2
#define SPACE_CHAR 32

#define IS_UPPER_CASE_LETTER(n) ((n) >= 'A' && (n) <= 'Z')
// from "!"(33) to "~"(126)
#define IS_PRINTABLE_ASCII(n) ((n) >= '!' && (n) <= '~')

typedef state (*handler) (pop3 * datos, char* arg1, char* arg2);

typedef struct CommandCDT
{
    state state;
    char command_name[MAX_COMMAND_LENGHT];
    handler execute; 
    int argCount;
    char arg1[MAX_ARG_LENGHT];
    char arg2[MAX_ARG_LENGHT];
} CommandCDT;

static bool readCommandArg(Command command, char * arg, buffer * b);

state user_handler(pop3 * datos, char* arg1, char* arg2){
    printf("user handler\n");
    return AUTHORIZATION;
}

state stat_handler(pop3* data, char* arg1, char* arg2) {
    // manejar stat
    // +OK <num_msg> <size_mailbox_in_octet>
    printf("stat handler\n");
    return TRANSACTION;
}


static const CommandCDT commands[COMMAND_COUNT] = {
    { .state = AUTHORIZATION , .command_name = "USER" , .execute = user_handler, .argCount = 1 },
    { .state = TRANSACTION, .command_name = "STAT" , .execute = stat_handler, .argCount = 0 },
};

static Command findCommand(const char* name, const state current) {
    for(int i = 0; i < COMMAND_COUNT; i++) {
        if(commands[i].state == current && strncasecmp(name,commands[i].command_name, MAX_COMMAND_LENGHT) == 0){
            Command command = malloc(sizeof(CommandCDT));
            if (command == NULL)
                return NULL;
            command->state = current;
            strncpy(command->command_name, name, MAX_COMMAND_LENGHT);
            command->execute = commands[i].execute;
            command->argCount = commands[i].argCount;
            return command;
        }
    }
    return NULL;
}


Command getCommand(buffer *b, const state current) {
    //los comandos en pop3 son de 4 caracteres (case insensitive)
    char commandName[MAX_COMMAND_LENGHT] = {0};
    int i = 0;
    for (; i < MAX_COMMAND_LENGHT && buffer_can_read(b); i++) {
        char c = (char) buffer_read(b);
        if (!IS_UPPER_CASE_LETTER(c)) // todo cambiar a alphabetic
            return NULL;
        commandName[i] = c;
    }
    Command command = findCommand(commandName, current);
    if (command == NULL) {
        return NULL;
    }

    if (command->argCount > 0) {
        if (!readCommandArg(command, command->arg1, b))
            return NULL;
    }

    if (command->argCount > 1) {
        if (!readCommandArg(command, command->arg2, b))
            return NULL;
    }
    if (!buffer_can_read(b) || buffer_read(b) != '\n') {
        free(command);
        return NULL;
    }
    return command;
}

state runCommand(Command command, pop3* datos) {
    if (command == NULL) {
        return ERROR;
    }

    state newState = command->execute(datos, command->arg1, command->arg2);
    free(command);
    return newState;
}

static bool readCommandArg(Command command, char * arg, buffer * b) {
    if (!buffer_can_read(b) || buffer_read(b) != SPACE_CHAR) {
        free(command);
        return false;
    }

    for (int j = 0; j < MAX_ARG_LENGHT; j++) {
        char c = (char) buffer_read(b);
        if (c == SPACE_CHAR) {
            if (j == 0) {
                free(command);
                return false;
            }
            break; // todo USER_foasjfoas_\n
        }
        if (!IS_PRINTABLE_ASCII(c)) {
            free(command);
            return false;
        }
        arg[j] = c;
    }
    // todo '0'
    return true;
}
