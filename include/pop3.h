#ifndef POP3_H
#define POP3_H
#include "args.h"
#include "buffer.h"
#include "selector.h"
#include "stm.h"
#include <netdb.h>

#define BUFFER_SIZE 521 // TODO : revisar tamaño

enum pop3_states {
  GREETING,
  // For USER,  CAPA and QUIT commands
  AUTHORIZATION,
  // For PASS and QUIT
  AUTHORIZATION_PASS,
  // For bla bla commands
  TRANSACTION,
  // MUst delete all dell mails and quit
  UPDATE,
  // If needed, add more states here

  // Final states
  ERROR,
  FINISH,
};

// ==============
// Temp Structure
typedef enum { NEW, READ, DELETED } MailState;

typedef struct Mail {
  unsigned number;
  const char* path;
  int nbytes;
  MailState state;
} Mail;

typedef struct {
  Mail* array;
  int length;
} MailArray;

MailArray* maildirInit(char* username, char* maildir);
void maildirFree(MailArray* mailArray);
int maildirGetTotalSize();
// Temp Structure
// ==============

typedef enum pop3_states state;

typedef struct pop3 {
  struct state_machine stm;
  uint8_t writeData[BUFFER_SIZE], readData[BUFFER_SIZE];
  buffer *writeBuff, *readBuff;
  User user;
  Mail mail;
} pop3;

void pop3_passive_accept(struct selector_key* key);

#endif
