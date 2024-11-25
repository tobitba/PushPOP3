#ifndef MAILDIR_H
#define MAILDIR_H

#include <stddef.h>
typedef enum { NEW, SEEN, DELETED } MailState;

typedef struct Mail {
  size_t number;
  char* path;
  size_t nbytes;
  MailState state;
} Mail;

typedef struct {
  Mail* array;
  size_t capacity;
  size_t length;
} MailArray;

MailArray* maildirInit(char* username, char* maildir);
void maildirFree(MailArray* mails);
int maildirGetTotalSize();

#endif
