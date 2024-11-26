#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>

#define MAX_USERS 10
#define MAX_ARG_LENGHT 40

typedef struct User {
  char* name;
  char* pass;
} User;

struct doh {
  char* host;
  char* ip;
  unsigned short port;
  char* path;
  char* query;
};

typedef struct {
  char* pop3Addr; // TODO: revisar bien...
  unsigned short pop3Port;

  char* push3Addr;
  unsigned short push3Port;

  struct doh doh;
  User users[MAX_USERS];
  int userCount;

  const char* maildirPath;
} pop3args;

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void parseArgs(const int argc, char** argv, pop3args* args);

#endif
