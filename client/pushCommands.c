#include "../include/pushCommands.h"
#include "../include/buffer.h"
#include "../include/push3.h"
#include "../include/authenticator.h"
#include "../include/serverMetrics.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define OK "+OK\r\n"
#define MAX_RESPONSE_LENGHT 2096
#define MAX_COMMAND_LENGHT 20
#define MAX_ARG_LENGHT 40
#define COMMAND_COUNT 3
#define SPACE_CHAR 32
#define ENTER_CHAR '\n'
#define CARRIAGE_RETURN_CHAR '\r'
#define LOW_BAR_CHAR '_'

#define IS_UPPER_CASE_LETTER(n) ((n) >= 'A' && (n) <= 'Z')
#define IS_LOWER_CASE_LETTER(n) ((n) >= 'a' && (n) <= 'z')
#define IS_ALPHABET(n) (IS_LOWER_CASE_LETTER(n) || IS_UPPER_CASE_LETTER(n))

// from "!"(33) to "~"(126)
#define IS_PRINTABLE_ASCII(n) ((n) >= '!' && (n) <= '~')

typedef struct arg_type {
  char value[MAX_ARG_LENGHT + 1];
  bool isArgPresent;
} arg_type;

typedef push3_state (*handler)(push3* datos, arg_type arg1, arg_type arg2);

typedef struct PushCommandCDT {
  push3_state state;
  char command_name[MAX_COMMAND_LENGHT + 1];
  handler execute;
  int argCount;
  arg_type arg1;
  arg_type arg2;
} PushCommandCDT;

static bool readPushCommandArg(PushCommand command, arg_type* arg, buffer* b);
static bool pushCommandContextValidation(PushCommand command, push3* data);

void writeOnUserBuffer2(buffer* b, char* str) {
  size_t lenght;
  uint8_t* buf = buffer_write_ptr(b, &lenght);
  memcpy(buf, str, strlen(str) + 1);
  buffer_write_adv(b, lenght);
}

push3_state loginHandler(push3* data, arg_type arg1, arg_type arg2) {
    if (!arg1.isArgPresent) {
      writeOnUserBuffer2(data->writeBuff, "->FAIL: Missing username\r\n");
      return PUSH_AUTHORIZATION;
    }
    if (!arg2.isArgPresent) {
      writeOnUserBuffer2(data->writeBuff, "->FAIL: Missing password\r\n");
      return PUSH_AUTHORIZATION;
    }

    if (!isUserAndPassValid(arg1.value, arg2.value)) {
      writeOnUserBuffer2(data->writeBuff, "->FAIL: Invalid username or password\r\n");
      return PUSH_AUTHORIZATION;
    }

    if (data->user.name == NULL) {
      data->user.name = calloc(1, MAX_ARG_LENGHT);
    }
    strcpy(data->user.name, arg1.value);

    if (data->user.pass == NULL) {
          data->user.pass = calloc(1, MAX_ARG_LENGHT);
    }
    strcpy(data->user.pass, arg1.value);

    writeOnUserBuffer2(data->writeBuff, "->SUCCESS: Logged in to the server\r\n");
    return PUSH_TRANSACTION;
}

push3_state getMetricsHandler(push3* data, arg_type arg1, arg_type arg2) {
    char reply[MAX_RESPONSE_LENGHT + 1];
    sprintf(reply, "->SUCCESS:\nCurrent connections count: %ld\nTotal connections count: %ld\nTotal bytes from mails read: %ld\r\n", getCurrentConnections(), getTotalConnections(), getTotalReadBytes());
    writeOnUserBuffer2(data->writeBuff, reply);
    return PUSH_TRANSACTION;
}

push3_state finishHandler(push3* data, arg_type arg1, arg_type arg2) {
    writeOnUserBuffer2(data->writeBuff, "->SUCCESS: Closing connection...\r\n");
    return PUSH_DONE;
}

static const PushCommandCDT commands[COMMAND_COUNT] = {
  {.state = PUSH_AUTHORIZATION, .command_name = "LOGIN", .execute = loginHandler, .argCount = 2},
  {.state = PUSH_TRANSACTION, .command_name = "METRICS", .execute = getMetricsHandler, .argCount = 0},
  {.state = PUSH_ANYWHERE, .command_name = "FINISH", .execute = finishHandler, .argCount = 0}
};

static PushCommand findPushCommand(const char* name) {
  for (int i = 0; i < COMMAND_COUNT; i++) {
    if (strncasecmp(name, commands[i].command_name, MAX_COMMAND_LENGHT) == 0) {
      PushCommand command = malloc(sizeof(PushCommandCDT));
      if (command == NULL) return NULL;
      command->state = commands[i].state;
      strncpy(command->command_name, name, MAX_COMMAND_LENGHT + 1);
      command->execute = commands[i].execute;
      command->argCount = commands[i].argCount;
      command->arg1.isArgPresent = false;
      command->arg2.isArgPresent = false;
      return command;
    }
  }
  return NULL;
}

PushCommand getPushCommand(buffer* b, push3_state current) {
  // los comandos en pop3 son de 4 caracteres (case insensitive)
  char commandName[MAX_COMMAND_LENGHT + 1] = {0};
  int i = 0;
  for (; i < MAX_COMMAND_LENGHT && buffer_can_read(b); i++) {
    char c = (char)buffer_peak(b);
    if (c == SPACE_CHAR || c == CARRIAGE_RETURN_CHAR) {
      if (i == 0) {
        buffer_reset(b);
        return NULL;
      }
      break;
    }

    c = (char)buffer_read(b);
    if (!IS_ALPHABET(c) && c != LOW_BAR_CHAR) {
      buffer_reset(b);
      return NULL;
    };
    commandName[i] = c;
  }
  PushCommand command = findPushCommand(commandName);
  if (command == NULL) {
    buffer_reset(b);
    return NULL;
  }

  if (command->argCount > 0) {
    if (!readPushCommandArg(command, &(command->arg1), b)) {
      free(command);
      buffer_reset(b);
      return NULL;
    }
  }

  if (command->argCount > 1) {
    if (!readPushCommandArg(command, &(command->arg2), b)) {
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

push3_state runPushCommand(PushCommand command, push3* data) {
  if (!pushCommandContextValidation(command, data)) {
    return data->stm.current->state;
  }

  push3_state newState = command->execute(data, command->arg1, command->arg2);
  free(command);
  return newState;
}

static bool readPushCommandArg(PushCommand command, arg_type* arg, buffer* b) {
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
      if (j == 0)
        return false;
      break;
    }
    c = (char)buffer_read(b);
    if (!IS_PRINTABLE_ASCII(c))
      return false;

    arg->value[j] = c;
  }
  arg->value[j] = '\0';
  arg->isArgPresent = true;
  return true;
}

static bool pushCommandContextValidation(PushCommand command, push3* data) {
  push3_state currentState = data->stm.current->state;
  if (command == NULL) {
    writeOnUserBuffer2(data->writeBuff, "->FAIL: Invalid command\r\n");
    return false;
  }

  if (command->state == PUSH_ANYWHERE) return true;

  if (currentState == PUSH_TRANSACTION && command->state != PUSH_TRANSACTION) {
    writeOnUserBuffer2(data->writeBuff, "->FAIL: You are already logged in! Try other commands\r\n");
    return false;
  }

  if (currentState != PUSH_TRANSACTION && command->state == PUSH_TRANSACTION) {
    writeOnUserBuffer2(data->writeBuff, "->FAIL: You must be logged in to use this command. Try running LOGIN <user> <pass>\r\n");
    return false;
  }

  if (currentState != command->state) {
    writeOnUserBuffer2(data->writeBuff, "->FAIL: You don´t have access to this command\r\n");
    return false;
  }

  return true;
}