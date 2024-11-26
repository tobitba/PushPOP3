#include "../include/commands.h"
#include "../include/args.h"
#include "../include/authenticator.h"
#include "../include/buffer.h"
#include "../include/maildir.h"
#include "../include/pop3.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define OK "+OK\r\n"
#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))
#define MAX_RESPONSE_LENGTH 512
#define MAX_COMMAND_LENGHT 4
#define MAX_ARG_LENGHT 40
#define SPACE_CHAR 32
#define ENTER_CHAR '\n'
#define CARRIAGE_RETURN_CHAR '\r'

#define IS_UPPER_CASE_LETTER(n) ((n) >= 'A' && (n) <= 'Z')
#define IS_LOWER_CASE_LETTER(n) ((n) >= 'a' && (n) <= 'z')
#define IS_ALPHABET(n) (IS_LOWER_CASE_LETTER(n) || IS_UPPER_CASE_LETTER(n))

// from "!"(33) to "~"(126)
#define IS_PRINTABLE_ASCII(n) ((n) >= '!' && (n) <= '~')

extern pop3args args;

typedef state (*handler)(pop3* data, char* arg, bool isArgPresent);
state noopHandler(pop3* data, char* _arg, bool _argPresent);
state userHandler(pop3* data, char* arg, bool argPresent);
state passHandler(pop3* data, char* arg, bool argPresent);
state statHandler(pop3* data, char* arg, bool _);
state listHandler(pop3* data, char* arg, bool argPresent);
state retrHandler(pop3* data, char* arg, bool argPresent);

typedef struct CommandCDT {
  state state;
  char command_name[MAX_COMMAND_LENGHT + 1];
  handler execute;
  int argCount;
  char arg[MAX_ARG_LENGHT + 1];
  bool isArgPresent;
} CommandCDT;

typedef enum { USER, PASS, STAT, NOOP, LIST, RETR } CommandNames;

static CommandCDT commands[] = {
  [USER] = {.state = AUTHORIZATION, .command_name = "USER", .execute = userHandler, .argCount = 1},
  [PASS] = {.state = AUTHORIZATION_PASS, .command_name = "PASS", .execute = passHandler, .argCount = 1},
  [STAT] = {.state = TRANSACTION, .command_name = "STAT", .execute = statHandler, .argCount = 0},
  [NOOP] = {.state = TRANSACTION, .command_name = "NOOP", .execute = noopHandler, .argCount = 0},
  [LIST] = {.state = TRANSACTION, .command_name = "LIST", .execute = listHandler, .argCount = 1},
  [RETR] = {.state = TRANSACTION, .command_name = "RETR", .execute = retrHandler, .argCount = 1},
};

static bool readCommandArg(Command command, char* arg, bool* isArgPresent, buffer* b);
static bool commandContextValidation(Command command, pop3* data);

bool writeResponse(pop3* data, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  char response[MAX_RESPONSE_LENGTH + 1];
  int length = vsnprintf(response, MAX_RESPONSE_LENGTH + 1, fmt, args);
  va_end(args);
  if (length < 0) {
    fprintf(stderr, "vsnprintf failed\n");
    exit(EXIT_FAILURE);
  } else if (length > MAX_RESPONSE_LENGTH) {
    printf("Response truncated\n");
    length = MAX_RESPONSE_LENGTH;
  }
  size_t availableWriteLength;
  uint8_t* buf = buffer_write_ptr(data->writeBuff, &availableWriteLength);
  if (availableWriteLength < (size_t)length) return false;
  memcpy(buf, response, length);
  buffer_write_adv(data->writeBuff, length);
  return true;
}

state noopHandler(pop3* data, char* _arg, bool _argPresent) {
  puts("Noop handler");
  writeResponse(data, OK);
  return TRANSACTION;
}

state userHandler(pop3* data, char* arg, bool argPresent) {
  puts("User handler");
  if (!argPresent) {
    writeResponse(data, "-ERR Missing username\r\n");
    return AUTHORIZATION;
  }
  if (data->user.name == NULL) {
    data->user.name = calloc(1, MAX_ARG_LENGHT);
  }
  strcpy(data->user.name, arg);
  writeResponse(data, OK);
  return AUTHORIZATION_PASS;
}

state passHandler(pop3* data, char* arg, bool argPresent) {
  puts("Pass handler");
  if (!argPresent) {
    writeResponse(data, "-ERR Missing password\r\n");
    return AUTHORIZATION_PASS;
  }
  if (isUserAndPassValid(data->user.name, arg)) {
    if (data->user.pass == NULL) {
      data->user.pass = calloc(1, MAX_ARG_LENGHT);
    }
    strcpy(data->user.pass, arg);
    data->mails = maildirInit(data->user.name, args.maildirPath);
    writeResponse(data, "+OK maildrop locked and ready\r\n");
    return TRANSACTION; // User logged succesfully
  }
  printf("ret errorr :(  la pass recibida es: %s\n", arg);
  writeResponse(data, "-ERR Invalid user & pass combination, try again\r\n");
  return AUTHORIZATION;
}

state statHandler(pop3* data, char* arg, bool _) {
  puts("Stat handler");
  writeResponse(data, "+OK %lu %lu\r\n", data->mails->length, maildirGetTotalSize(data->mails));
  return TRANSACTION;
}

size_t listMailsTillBufferFull(pop3* data, size_t startIdx) {
  size_t mailCount = data->mails->length;
  size_t i;
  for (i = startIdx; i < mailCount; ++i) {
    Mail mail = data->mails->array[i];
    if (mail.state != DELETED) {
      if (!writeResponse(data, "%lu %lu\r\n", mail.number, mail.nbytes)) {
        break;
      }
    }
  }
  return i;
}

state listHandler(pop3* data, char* arg, bool argPresent) {
  if (data->stm.current->state == PENDING_RESPONSE) {
    *(size_t*)data->pendingData = listMailsTillBufferFull(data, *(int*)data->pendingData);
    if (*(size_t*)data->pendingData < data->mails->length) return PENDING_RESPONSE;
    data->pendingCommand = NULL;
    free(data->pendingData);
  } else {
    puts("LIST handler");
    if (argPresent) {
      size_t mailNumber = atoi(arg);
      if (mailNumber == 0 || mailNumber > data->mails->length) {
        writeResponse(data, "-ERR no such message\r\n");
        return TRANSACTION;
      }
      writeResponse(data, "+OK %i %lu\r\n", mailNumber, data->mails->array[mailNumber - 1].nbytes);
    } else {
      size_t mailCount = data->mails->length;
      writeResponse(data, "+OK %lu messages (%lu octects)\r\n", mailCount, maildirGetTotalSize(data->mails));
      size_t listedCount = listMailsTillBufferFull(data, 0);
      if (listedCount < mailCount) {
        data->pendingCommand = &commands[LIST];
        data->pendingData = malloc(sizeof(size_t));
        *(size_t*)data->pendingData = listedCount;
        return PENDING_RESPONSE;
      }
    }
  }
  if (!writeResponse(data, ".\r\n")) return PENDING_RESPONSE;
  return TRANSACTION;
}

state retrHandler(pop3* data, char* arg, bool argPresent) {
  FILE* file;
  if (data->stm.current->state == PENDING_RESPONSE) {
    file = data->pendingData;
  } else {
    if (!argPresent) {
      writeResponse(data, "-ERR missing argument\r\n");
      return TRANSACTION;
    }
    size_t mailNumber = atoi(arg);
    if (mailNumber == 0 || mailNumber > data->mails->length) {
      writeResponse(data, "-ERR no such message\r\n");
      return TRANSACTION;
    }
    Mail mail = data->mails->array[mailNumber - 1];
    if (mail.state == DELETED) {
      writeResponse(data, "-ERR the mail was marked to be deleted\r\n");
      return TRANSACTION;
    }
    file = fopen(mail.path, "r");
    maildirMarkAsSeen(data->mails, mailNumber);
  }
  size_t availableWriteLength;
  uint8_t* buf = buffer_write_ptr(data->writeBuff, &availableWriteLength);
  size_t length = fread(buf, 1, availableWriteLength, file);
  buffer_write_adv(data->writeBuff, length);
  if (feof(file)) {
    if (!writeResponse(data, ".\r\n")) return PENDING_RESPONSE;
    fclose(file);
    return TRANSACTION;
  } else {
    data->pendingCommand = &commands[RETR];
    data->pendingData = file;
    return PENDING_RESPONSE;
  }
}

static Command findCommand(const char* name) {
  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    if (strncasecmp(name, commands[i].command_name, MAX_COMMAND_LENGHT) == 0) {
      commands[i].isArgPresent = false;
      return commands + i;
    }
  }
  return NULL;
}

Command getCommand(buffer* b, const state current) {
  // los comandos en pop3 son de 4 caracteres (case insensitive)
  char commandName[MAX_COMMAND_LENGHT + 1] = {0};
  for (int i = 0; i < MAX_COMMAND_LENGHT && buffer_can_read(b); i++) {
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
    if (!readCommandArg(command, command->arg, &(command->isArgPresent), b)) {
      buffer_reset(b);
      return NULL;
    }
  }

  if (!buffer_can_read(b) || buffer_read(b) != CARRIAGE_RETURN_CHAR) {
    buffer_reset(b);
    return NULL;
  }

  if (!buffer_can_read(b) || buffer_read(b) != ENTER_CHAR) {
    buffer_reset(b);
    return NULL;
  }

  buffer_reset(b);
  return command;
}

state runCommand(Command command, pop3* data) {
  if (!commandContextValidation(command, data)) {
    return data->stm.current->state;
  }

  printf("Running command: %s\n", command->command_name);
  state newState = command->execute(data, command->arg, command->isArgPresent);
  return newState;
}

state continuePendingCommand(pop3* data) {
  Command command = data->pendingCommand;
  printf("Continuing command: %s\n", command->command_name);
  state newState = command->execute(data, command->arg, command->isArgPresent);
  // if (newState != PENDING_RESPONSE) free(command);
  return newState;
}

static bool readCommandArg(Command command, char* arg, bool* isArgPresent, buffer* b) {
  // The argument might be optional, this will leave isArgPresent in false.
  if (buffer_can_read(b) && buffer_peak(b) == CARRIAGE_RETURN_CHAR) return true;

  if (!buffer_can_read(b) || buffer_read(b) != SPACE_CHAR) return false;

  int j = 0;
  for (; j < MAX_ARG_LENGHT; j++) {
    if (!buffer_can_read(b)) return false;

    char c = (char)buffer_peak(b);
    if (c == SPACE_CHAR || c == CARRIAGE_RETURN_CHAR) {
      // The argument after the first space was another space or enter. Ex: USER__ Or User_\r\n.
      if (j == 0) return false;
      // If j != 0, there's a word between the first space and this char, it could be followed by another
      // argument or end there if '\r'.
      break;
    }
    c = (char)buffer_read(b);
    if (!IS_PRINTABLE_ASCII(c)) return false;

    arg[j] = c;
  }
  arg[j] = '\0';
  *isArgPresent = true;
  return true;
}

static bool commandContextValidation(Command command, pop3* data) {
  if (command == NULL) {
    writeResponse(data, "-ERR Invalid command\r\n");
    return false;
  }
  state currentState = data->stm.current->state;

  if (command->state == ANYWHERE) return true;

  if (currentState == TRANSACTION && command->state != TRANSACTION) {
    writeResponse(data, "-ERR You are already logged in\r\n");
    return false;
  }

  if (currentState != TRANSACTION && command->state == TRANSACTION) {
    writeResponse(data, "-ERR You must be logged in to use this command\r\n");
    return false;
  }

  if (currentState == AUTHORIZATION && command->state == AUTHORIZATION_PASS) {
    writeResponse(data, "-ERR You must issue a USER command first\r\n");
    return false;
  }

  if (currentState == AUTHORIZATION_PASS && command->state == AUTHORIZATION) {
    writeResponse(data, "-ERR You've already picked a User, try a password\r\n");
    return false;
  }

  if (currentState != command->state) {
    writeResponse(data, "-ERR You don't have access to this command\r\n");
    return false;
  }

  return true;
}
