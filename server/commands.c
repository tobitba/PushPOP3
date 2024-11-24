#include "../include/pop3.h"
#include "../include/buffer.h"
#include <strings.h>
#define MAX_COMMAND_LENGHT 5
#define COMMAND_COUNT 1

typedef state (*handler) (pop3 * datos, char* arg1, char* arg2);

typedef struct pop_command
{
    enum pop3_states state;
    char command_name[MAX_COMMAND_LENGHT];
    handler execute; 
    int argCount;
} pop_command;

state user_handler(pop3 * datos, char* arg1, char* arg2){
    return AUTHORIZATION;
}


static const pop_command commands[COMMAND_COUNT] = {

    { .state = AUTHORIZATION , .command_name = "USER" , .execute = user_handler, .argCount = 1 },

};

handler getCommandHandler(char* name, state current){
    int i = 0;
    while(i < COMMAND_COUNT){
        if(commands[i].state == current && strncasecmp(name,commands[i].command_name, MAX_COMMAND_LENGHT)){
            return commands[i].execute;
        }
    }
    return NULL;
}


int getCommand(buffer *b, state current){
    //los comandos en popo3 son de 4 caracteres (case insensitive)
    char command[MAX_COMMAND_LENGHT] = {0};
    int i = 0;
    while(i < MAX_COMMAND_LENGHT && buffer_can_read(b)){
        command[i] = buffer_read(b);
    }
    return command[0];
}

