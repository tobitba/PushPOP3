#include "../include/maildir.h"
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NEW_PATH "new"
#define SEEN_PATH "cur"
#define INITIAL_CAPACITY 30
#define CAPACITY_INCREMENT 15

void _addMails(const char* path, MailArray* mailArray, MailState state);
void _maildirAddMail(MailArray* mails, char* path, MailState state, size_t nbytes);

MailArray* maildirInit(char* username, const char* maildir) {
  MailArray* mails = malloc(sizeof(MailArray));
  mails->array = malloc(sizeof(Mail) * INITIAL_CAPACITY);
  mails->capacity = INITIAL_CAPACITY;
  mails->length = 0;

  char pathNew[PATH_MAX];
  char pathSeen[PATH_MAX];
  if (snprintf(pathNew, PATH_MAX, "%s/%s/%s", maildir, username, NEW_PATH) < 0 ||
      snprintf(pathSeen, PATH_MAX, "%s/%s/%s", maildir, username, SEEN_PATH) < 0) {
    // TODO: do we need to handle this better?
    exit(1);
  }

  _addMails(pathNew, mails, NEW);
  _addMails(pathSeen, mails, SEEN);

  return mails;
}

void maildirFree(MailArray* mails) {
  for (size_t i = 0; i < mails->length; ++i) free(mails->array[i].path);
  free(mails->array);
  free(mails);
}

size_t maildirGetTotalSize(MailArray* mails) {
  size_t size = 0;
  for (size_t i = 0; i < mails->length; ++i) size += mails->array[i].nbytes;
  return size;
}

void _maildirAddMail(MailArray* mails, char* path, MailState state, size_t nbytes) {
  Mail mail;
  mail.path = path;
  mail.state = state;
  mail.nbytes = nbytes;
  if (mails->length == mails->capacity) {
    Mail* aux = realloc(mails->array, mails->capacity + CAPACITY_INCREMENT);
    if (aux == NULL) {
      // TODO: do we need to handle this better?
      exit(EXIT_FAILURE);
    }
    mails->array = aux;
  }
  mail.number = mails->length + 1;
  mails->array[mails->length] = mail;
  ++mails->length;
}

void _addMails(const char* path, MailArray* mails, MailState state) {
  DIR* dir = opendir(path);
  if (!dir) {
    fprintf(stderr, "Failed to open directory %s: %s\n", path, strerror(errno));
    // TODO: error handling.
    exit(EXIT_FAILURE);
  }
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    struct stat stats;
    char* filePath = malloc(PATH_MAX);
    // TODO: better error handling?
    if (filePath == NULL) exit(EXIT_FAILURE);
    if (snprintf(filePath, PATH_MAX, "%s/%s", path, entry->d_name) < 0) exit(EXIT_FAILURE);
    if (lstat(filePath, &stats) < 0) exit(EXIT_FAILURE);
    if (S_ISREG(stats.st_mode)) _maildirAddMail(mails, filePath, state, stats.st_size);
    else free(filePath);
  }

  closedir(dir);
}
