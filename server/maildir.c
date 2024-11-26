#include "../include/maildir.h"
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
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
void _maildirAddMail(MailArray* mails, char* filename, char* path, MailState state, size_t nbytes);

MailArray* maildirInit(char* username, const char* maildir) {
  MailArray* mails = malloc(sizeof(MailArray));
  mails->array = malloc(sizeof(Mail) * INITIAL_CAPACITY);
  mails->capacity = INITIAL_CAPACITY;
  mails->length = 0;

  mails->pathNew = malloc(PATH_MAX + 1);
  mails->pathSeen = malloc(PATH_MAX + 1);
  if (snprintf(mails->pathNew, PATH_MAX, "%s/%s/%s", maildir, username, NEW_PATH) < 0 ||
      snprintf(mails->pathSeen, PATH_MAX, "%s/%s/%s", maildir, username, SEEN_PATH) < 0) {
    // TODO: do we need to handle this better?
    exit(1);
  }

  _addMails(mails->pathNew, mails, NEW);
  _addMails(mails->pathSeen, mails, SEEN);

  return mails;
}

void maildirFree(MailArray* mails) {
  for (size_t i = 0; i < mails->length; ++i) {
    free(mails->array[i].filename);
    free(mails->array[i].path);
  }
  free(mails->array);
  free(mails->pathNew);
  free(mails->pathSeen);
  free(mails);
}

bool maildirMarkAsSeen(MailArray* mails, size_t mailNumber) {
  if (mails == NULL || mailNumber == 0) return false;
  Mail* mail = mails->array + mailNumber - 1;
  if (mail->state != NEW || mail->markedDeleted) return false;
  mail->state = SEEN;
  char* newPath = malloc(PATH_MAX + 1);
  snprintf(newPath, PATH_MAX, "%s/%s", mails->pathSeen, mail->filename);
  if (rename(mail->path, newPath) < 0) {
    perror("Error moving file");
    return false;
  }
  free(mail->path);
  mail->path = newPath;
  return true;
}

size_t maildirNonDeletedCount(MailArray* mails) {
  size_t size = 0;
  for (size_t i = 0; i < mails->length; ++i) {
    if (!mails->array[i].markedDeleted) size += 1;
  }
  return size;
}

size_t maildirGetTotalSize(MailArray* mails) {
  size_t size = 0;
  for (size_t i = 0; i < mails->length; ++i) {
    if (!mails->array[i].markedDeleted) size += mails->array[i].nbytes;
  }
  return size;
}

int maildirDeleteMarked(MailArray* mails) {
  bool errors = 0;
  for (size_t i = 0; i < mails->length; ++i) {
    if (mails->array[i].markedDeleted) {
      if (remove(mails->array[i].path) < 0) {
        perror("Error deleting file");
        errors = -1;
      }
    }
  }
  return errors;
}

void _maildirAddMail(MailArray* mails, char* filename, char* path, MailState state, size_t nbytes) {

  if (mails->length == mails->capacity) {
    mails->capacity += CAPACITY_INCREMENT;
    Mail* aux = realloc(mails->array, mails->capacity * sizeof(Mail));
    if (aux == NULL) {
      // TODO: do we need to handle this better?
      exit(EXIT_FAILURE);
    }
    mails->array = aux;
  }
  Mail* mail = mails->array + mails->length;
  mail->markedDeleted = false;
  mail->filename = filename;
  mail->path = path;
  mail->state = state;
  mail->nbytes = nbytes;
  mail->number = mails->length + 1;
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
    char* filePath = malloc(PATH_MAX + 1);
    // TODO: better error handling?
    if (filePath == NULL) exit(EXIT_FAILURE);
    if (snprintf(filePath, PATH_MAX, "%s/%s", path, entry->d_name) < 0) exit(EXIT_FAILURE);
    if (lstat(filePath, &stats) < 0) exit(EXIT_FAILURE);
    if (S_ISREG(stats.st_mode)) {
      char* filename = malloc(PATH_MAX + 1);
      strncpy(filename, entry->d_name, PATH_MAX);
      filename[PATH_MAX] = 0;
      _maildirAddMail(mails, filename, filePath, state, stats.st_size);
    } else free(filePath);
  }

  closedir(dir);
}
