#include "../include/commands.h"
#include "../include/authenticator.h"
#include "../include/buffer.h"
#include "../include/pop3.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define OK "+OK\r\n"
#define MAX_COMMAND_LENGHT 4
#define MAX_ARG_LENGHT 40
#define COMMAND_COUNT 4
#define SPACE_CHAR 32
#define ENTER_CHAR '\n'
#define CARRIAGE_RETURN_CHAR '\r'

#define IS_UPPER_CASE_LETTER(n) ((n) >= 'A' && (n) <= 'Z')
#define IS_LOWER_CASE_LETTER(n) ((n) >= 'a' && (n) <= 'z')
#define IS_ALPHABET(n) (IS_LOWER_CASE_LETTER(n) || IS_UPPER_CASE_LETTER(n))

// from "!"(33) to "~"(126)
#define IS_PRINTABLE_ASCII(n) ((n) >= '!' && (n) <= '~')

typedef state (*handler)(pop3* datos, char* arg1, bool isArg1Present);

typedef struct CommandCDT {
  state state;
  char command_name[MAX_COMMAND_LENGHT + 1];
  handler execute;
  int argCount;
  char arg1[MAX_ARG_LENGHT + 1];
  bool isArg1Present;
} CommandCDT;

static bool readCommandArg(Command command, char* arg, bool* isArgPresent, buffer* b);
static bool commandContextValidation(Command command, pop3* data);

void writeOnUserBuffer(buffer* b, char* str) {
  size_t lenght;
  uint8_t* buf = buffer_write_ptr(b, &lenght);
  memcpy(buf, str, strlen(str) + 1);
  buffer_write_adv(b, lenght);
}

state noopHandler(pop3* data, char* arg1, bool isArgPresent) {
  puts("Noop handler");
  writeOnUserBuffer(data->writeBuff, OK);
  return TRANSACTION;
}

state userHandler(pop3* data, char* arg1, bool isArg1Present) {
  puts("User handler");
  if (!isArg1Present) {
    writeOnUserBuffer(data->writeBuff, "-ERR Missing username\r\n");
    return AUTHORIZATION;
  }
  if (data->user.name == NULL) {
    data->user.name = calloc(1, MAX_ARG_LENGHT);
  }
  strcpy(data->user.name, arg1);
  writeOnUserBuffer(data->writeBuff, OK);
  return AUTHORIZATION_PASS;
}

state passHandler(pop3* data, char* arg1, bool isArg1Present) {
  puts("Pass handler");
  if (!isArg1Present) {
    writeOnUserBuffer(data->writeBuff, "-ERR Missing password\r\n");
    return AUTHORIZATION_PASS;
  }
  if (isUserAndPassValid(data->user.name, arg1)) {
    if (data->user.pass == NULL) {
      data->user.pass = calloc(1, MAX_ARG_LENGHT);
    }
    strcpy(data->user.pass, arg1);
    // TODO: armar estructura de mails del user y verificar dirs...
    writeOnUserBuffer(data->writeBuff, "+OK maildrop locked and ready\r\n");
    return TRANSACTION; // User logged succesfully
  }
  printf("ret errorr :(  la pass recibida es: %s\n", arg1);
  writeOnUserBuffer(data->writeBuff, "-ERR Invalid user & pass combination, try again\r\n");
  return AUTHORIZATION;
}

state stat_handler(pop3* data, char* arg1, bool isArg1Present) {
  // manejar stat
  // +OK <num_msg> <size_mailbox_in_octet>
  printf("stat handler\n");
  return TRANSACTION;
}

static const CommandCDT commands[COMMAND_COUNT] = {
  {.state = AUTHORIZATION, .command_name = "USER", .execute = userHandler, .argCount = 1},
  {.state = AUTHORIZATION_PASS, .command_name = "PASS", .execute = passHandler, .argCount = 1},
  {.state = TRANSACTION, .command_name = "STAT", .execute = stat_handler, .argCount = 0},
  {.state = TRANSACTION, .command_name = "NOOP", .execute = noopHandler, .argCount = 0},
};

static Command findCommand(const char* name) {
  for (int i = 0; i < COMMAND_COUNT; i++) {
    if (strncasecmp(name, commands[i].command_name, MAX_COMMAND_LENGHT) == 0) {
      Command command = malloc(sizeof(CommandCDT));
      if (command == NULL) return NULL;
      command->state = commands[i].state;
      strncpy(command->command_name, name, MAX_COMMAND_LENGHT + 1);
      command->execute = commands[i].execute;
      command->argCount = commands[i].argCount;
      command->isArg1Present = false;
      return command;
    }
  }
  return NULL;
}

Command getCommand(buffer* b, const state current) {
  // los comandos en pop3 son de 4 caracteres (case insensitive)
  char commandName[MAX_COMMAND_LENGHT + 1] = {0};
  int i = 0;
  for (; i < MAX_COMMAND_LENGHT && buffer_can_read(b); i++) {
    char c = (char)buffer_read(b);
    if (!IS_ALPHABET(c)) {
      buffer_reset(b);
      return NULL;
    };
    commandName[i] = c;
  }
  Command command = findCommand(commandName);
  if (command == NULL) {
    buffer_reset(b);
    return NULL;
  }

  if (command->argCount > 0) {
    if (!readCommandArg(command, command->arg1, &(command->isArg1Present), b)) {
      free(command);
      buffer_reset(b);
      return NULL;
    }
  }

  if (!buffer_can_read(b) || buffer_read(b) != CARRIAGE_RETURN_CHAR) {
    free(command);
    buffer_reset(b);
    return NULL;
  }

  if (!buffer_can_read(b) || buffer_read(b) != ENTER_CHAR) {
    free(command);
    buffer_reset(b);
    return NULL;
  }

  buffer_reset(b);
  return command;
}

state runCommand(Command command, pop3* data) {
  if (!commandContextValidation(command, data))
    return data->stm.current->state;

  printf("Running command: %s\n", command->command_name);
  state newState = command->execute(data, command->arg1, command->isArg1Present);
  free(command);
  return newState;
}

static bool readCommandArg(Command command, char* arg, bool* isArgPresent, buffer* b) {
  if (buffer_can_read(b) && buffer_peak(b) == CARRIAGE_RETURN_CHAR) // the argument might be optional, this will leave isArgPresent in false
    return true;

  if (!buffer_can_read(b) || buffer_read(b) != SPACE_CHAR)
    return false;


  int j = 0;
  for (; j < MAX_ARG_LENGHT; j++) {
    if (!buffer_can_read(b))
      return false;

    char c = (char)buffer_peak(b);
    if (c == SPACE_CHAR || c == CARRIAGE_RETURN_CHAR) {
      if (j == 0)  // The argument after the first space was another space or enter. Ex: USER__ Or User_\r\n
        return false;
      break; // If j != 0, there's a word between the first space and this char, it could be followed by another
             // argument or end there if '\r'
    }
    c = (char)buffer_read(b);
    if (!IS_PRINTABLE_ASCII(c))
      return false;

    arg[j] = c;
  }
  arg[j] = '\0';
  *isArgPresent = true;
  return true;
}

static bool commandContextValidation(Command command, pop3* data) {
  state currentState = data->stm.current->state;
  if (command == NULL) {
    writeOnUserBuffer(data->writeBuff, "-ERR Invalid command\r\n");
    return false;
  }

  if (command->state == ANYWHERE)
    return true;

  if (currentState == TRANSACTION && command->state != TRANSACTION) {
    writeOnUserBuffer(data->writeBuff, "-ERR You are already logged in\r\n");
    return false;
  }

  if (currentState != TRANSACTION && command->state == TRANSACTION) {
    writeOnUserBuffer(data->writeBuff, "-ERR You must be logged in to use this command\r\n");
    return false;
  }

  if (currentState == AUTHORIZATION && command->state == AUTHORIZATION_PASS) {
    writeOnUserBuffer(data->writeBuff, "-ERR You must issue a USER command first\r\n");
    return false;
  }

  if (currentState == AUTHORIZATION_PASS && command->state == AUTHORIZATION) {
    writeOnUserBuffer(data->writeBuff, "-ERR You've already picked a User, try a password\r\n");
    return false;
  }

  if (currentState != command->state) {
    writeOnUserBuffer(data->writeBuff, "-ERR You don´t have access to this command\r\n");
    return false;
  }

  return true;
}