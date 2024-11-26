#ifndef POP3_H
#define POP3_H

#include "args.h"
#include "buffer.h"
#include "maildir.h"
#include "selector.h"
#include "stm.h"
#include <netdb.h>
#include <stdio.h>

#define BUFFER_SIZE 512 // TODO : revisar tamaño
typedef struct CommandCDT* Command;

enum pop3_states {
  GREETING,
  // For USER,  CAPA and QUIT commands
  AUTHORIZATION,
  // For PASS and QUIT
  AUTHORIZATION_PASS,
  // For bla bla commands
  TRANSACTION,
  // Can use this command in any state
  ANYWHERE,
  // MUst delete all dell mails and quit
  UPDATE,
  // If needed, add more states here

  PENDING_RESPONSE,

  // Final states
  ERROR,
  FINISH,
};

typedef enum pop3_states state;

typedef struct pop3 {
  struct state_machine stm;
  uint8_t writeData[BUFFER_SIZE], readData[BUFFER_SIZE];
  buffer *writeBuff, *readBuff;
  User user;
  MailArray* mails;
  Command pendingCommand;
  void* pendingData;
} pop3;

void pop3_passive_accept(struct selector_key* key);

#endif
