#ifndef MAILDIR_H
#define MAILDIR_H

#include <stdbool.h>
#include <stddef.h>
typedef enum { NEW, SEEN, DELETED } MailState;

typedef struct Mail {
  size_t number;
  char* filename;
  char* path;
  size_t nbytes;
  MailState state;
} Mail;

typedef struct {
  Mail* array;
  size_t capacity;
  size_t length;
  char* pathNew;
  char* pathSeen;
} MailArray;

MailArray* maildirInit(char* username, const char* maildir);
void maildirFree(MailArray* mails);
size_t maildirGetTotalSize(MailArray* mails);
bool maildirMarkAsSeen(MailArray* mails, size_t mailNumber);

#endif
