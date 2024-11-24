#include "../include/pop3.h"
#include "../include/buffer.h"
#include "../include/commands.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#define MAX_COMMAND_LENGHT 4
#define MAX_ARG_LENGHT 40
#define COMMAND_COUNT 2
#define SPACE_CHAR 32
#define ENTER_CHAR '\n'

#define IS_UPPER_CASE_LETTER(n) ((n) >= 'A' && (n) <= 'Z')
#define IS_LOWER_CASE_LETTER(n) ((n) >= 'a' && (n) <= 'z')
#define IS_ALPHABET(n) (IS_LOWER_CASE_LETTER(n) || IS_UPPER_CASE_LETTER(n))

// from "!"(33) to "~"(126)
#define IS_PRINTABLE_ASCII(n) ((n) >= '!' && (n) <= '~')

typedef state (*handler) (pop3 * datos, char* arg1, char* arg2);

typedef struct CommandCDT
{
    state state;
    char command_name[MAX_COMMAND_LENGHT + 1];
    handler execute; 
    int argCount;
    char arg1[MAX_ARG_LENGHT + 1];
    char arg2[MAX_ARG_LENGHT + 1];
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
    { .state = AUTHORIZATION, .command_name = "USER" , .execute = user_handler, .argCount = 1 },
    { .state = TRANSACTION, .command_name = "STAT" , .execute = stat_handler, .argCount = 0 },
};

static Command findCommand(const char* name, const state current) {
    for(int i = 0; i < COMMAND_COUNT; i++) {
        if(commands[i].state == current && strncasecmp(name,commands[i].command_name, MAX_COMMAND_LENGHT) == 0){
            Command command = malloc(sizeof(CommandCDT));
            if (command == NULL)
                return NULL;
            command->state = current;
            strncpy(command->command_name, name, MAX_COMMAND_LENGHT + 1);
            command->execute = commands[i].execute;
            command->argCount = commands[i].argCount;
            return command;
        }
    }
    return NULL;
}


Command getCommand(buffer *b, const state current) {
    //los comandos en pop3 son de 4 caracteres (case insensitive)
    char commandName[MAX_COMMAND_LENGHT + 1] = {0};
    int i = 0;
    for (; i < MAX_COMMAND_LENGHT && buffer_can_read(b); i++) {
        char c = (char) buffer_read(b);
        if (!IS_ALPHABET(c))
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
    if (!buffer_can_read(b) || buffer_read(b) != ENTER_CHAR) {
        free(command);
        return NULL;
    }
    return command;
}

state runCommand(Command command, pop3* datos) {
    if (command == NULL) {
        return ERROR;
    }

    printf("Running command: %s\n", command->command_name);
    state newState = command->execute(datos, command->arg1, command->arg2);
    free(command);
    return newState;
}

static bool readCommandArg(Command command, char * arg, buffer * b) {
    if (!buffer_can_read(b) || buffer_read(b) != SPACE_CHAR) {
        free(command);
        return false;
    }
    int j = 0;
    for (; j < MAX_ARG_LENGHT; j++) {
        char c = (char) buffer_peak(b);
        if (c == SPACE_CHAR || c == ENTER_CHAR) {
            if (j == 0) { // The argument after the firstspace was another space
                free(command);
                return false;
            }
            break; // If j != 0, there's a word between the first space and this one, it could be for another argument
        }
        c = (char) buffer_read(b);
        if (!IS_PRINTABLE_ASCII(c)) {
            free(command);
            return false;
        }
        arg[j] = c;
    }
    arg[j] = '\0';
    return true;
}
