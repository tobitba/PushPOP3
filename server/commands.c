#include "../include/pop3.h"
#include "../include/buffer.h"
#include "../include/commands.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "../include/authenticator.h"
#include <stdlib.h>
#define OK "+OK\n\r\0"
#define MAX_COMMAND_LENGHT 4
#define MAX_ARG_LENGHT 40
#define COMMAND_COUNT 3
#define SPACE_CHAR 32
#define ENTER_CHAR '\n'

#define IS_UPPER_CASE_LETTER(n) ((n) >= 'A' && (n) <= 'Z')
#define IS_LOWER_CASE_LETTER(n) ((n) >= 'a' && (n) <= 'z')
#define IS_ALPHABET(n) (IS_LOWER_CASE_LETTER(n) || IS_UPPER_CASE_LETTER(n))

// from "!"(33) to "~"(126)
#define IS_PRINTABLE_ASCII(n) ((n) >= '!' && (n) <= '~')

typedef state (*handler) (pop3 * datos, char* arg1, bool isArg1Present);

typedef struct CommandCDT
{
    state state;
    char command_name[MAX_COMMAND_LENGHT + 1];
    handler execute; 
    int argCount;
    char arg1[MAX_ARG_LENGHT + 1];
    bool isArg1Present;
} CommandCDT;

static bool readCommandArg(Command command, char * arg, bool * isArgPresent ,buffer * b);

void writeOnUserBuffer(buffer* b, char* str){
    size_t lenght;
    uint8_t *buf = buffer_write_ptr(b, &lenght);
    memcpy(buf, str, strlen(str)+1);
    buffer_write_adv(b, lenght);

}

state userHandler(pop3 * datos, char* arg1, bool isArg1Present){
    if(!isArg1Present) {
        return ERROR;
    }
    if(datos->user.name == NULL){
        datos->user.name = calloc(1,MAX_ARG_LENGHT);
    }
    strcpy(datos->user.name, arg1);
    writeOnUserBuffer(datos->writeBuff, OK);
    return AUTHORIZATION_PASS;
}

state passHandler(pop3 * datos, char* arg1, bool isArg1Present){
    printf("passHAndler\n");
    if(!isArg1Present) {
        return ERROR;
    }
    if(isUserAndPassValid(datos->user.name,arg1)){
         printf("passHAndler adentro if\n");
        if(datos->user.pass == NULL){
            datos->user.pass = calloc(1,MAX_ARG_LENGHT);
        }
        strcpy(datos->user.pass, arg1);        
        //TODO: armar estructura de mails del user y verificar dirs...
        writeOnUserBuffer(datos->writeBuff, "+OK maildrop locked and ready\r\n");
        return TRANSACTION; //User loghged succesfully
    }
     printf("ret errorr :(  la pass recibida es: %s\n", arg1);
   return ERROR; //TODO: add error msg
}


state stat_handler(pop3* data, char* arg1, bool isArg1Present) {
    // manejar stat
    // +OK <num_msg> <size_mailbox_in_octet>
    printf("stat handler\n");
    return TRANSACTION;
}


static const CommandCDT commands[COMMAND_COUNT] = {
    { .state = AUTHORIZATION, .command_name = "USER" , .execute = userHandler, .argCount = 1 },
    { .state = AUTHORIZATION_PASS, .command_name = "PASS" , .execute = passHandler, .argCount = 1 },
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
            command->isArg1Present = false;
            return command;
        }
    }
    return NULL;
}


Command getCommand(buffer *b, const state current) {
    //los comandos en pop3 son de 4 caracteres (case insensitive)
    char commandName[MAX_COMMAND_LENGHT + 1] = {0};
    int i = 0;
    printf("getcommand\n");
    for (; i < MAX_COMMAND_LENGHT && buffer_can_read(b); i++) {
        char c = (char) buffer_read(b);
        if (!IS_ALPHABET(c))
            return NULL;
        commandName[i] = c;
        printf("for c = %c\n",c);
    }
     printf("name %s\n", commandName);
    Command command = findCommand(commandName, current);
    printf("find commandpasado %s  \n", command->command_name);
    if (command == NULL) {
        return NULL;
    }

    if (command->argCount > 0) {
        if (!readCommandArg(command, command->arg1, &(command->isArg1Present), b))
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
    state newState = command->execute(datos, command->arg1, command->isArg1Present);
    free(command);
    return newState;
}

static bool readCommandArg(Command command, char * arg, bool * isArgPresent ,buffer * b) {
    if (buffer_peak(b) == ENTER_CHAR) // the argument might be optional, this will leave isArgPresent in false
        return true;

    if (!buffer_can_read(b) || buffer_read(b) != SPACE_CHAR) {
        free(command);
        return false;
    }

    int j = 0;
    for (; j < MAX_ARG_LENGHT; j++) {
        char c = (char) buffer_peak(b);
        if (c == SPACE_CHAR || c == ENTER_CHAR) {
            if (j == 0) { // The argument after the first space was another space or enter. Ex: USER__ Or User_\n
                free(command);
                return false;
            }
            break; // If j != 0, there's a word between the first space and this char, it could be followed by another argument or end there if '\n'
        }
        c = (char) buffer_read(b);
        if (!IS_PRINTABLE_ASCII(c)) {
            free(command);
            return false;
        }
        arg[j] = c;
    }
    arg[j] = '\0';
    *isArgPresent = true;
    return true;
}
