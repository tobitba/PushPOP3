#include <errno.h>
#include <getopt.h>
#include <limits.h> /* LONG_MIN et al */
#include <stdio.h>  /* for printf */
#include <stdlib.h> /* for exit */
#include <string.h> /* memset */
#include <sys/stat.h>

#include "../include/args.h"

static unsigned short port(const char* s) {
  char* end = 0;
  const long sl = strtol(s, &end, 10);

  if (end == s || '\0' != *end || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) || sl < 0 || sl > USHRT_MAX) {
    fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
    exit(1);
    return 1;
  }
  return (unsigned short)sl;
}

static User newUser(char* s) {
  User user;
  char* p = strchr(s, ':');
  if (p == NULL) {
    fprintf(stderr, "password not found\n");
    exit(1);
  } else {
    *p = 0;
    p++;
    user.name = s;
    user.pass = p;
  }
  return user;
}

static const char* validatedDirectory(const char* path) {
  struct stat path_stat;
  stat(path, &path_stat);
  if (!S_ISDIR(path_stat.st_mode)) {
    fprintf(stderr, "Path %s is not a directory", path);
    exit(1);
  }
  return path;
}

static void version(void) {
  fprintf(
    stderr, "PushPOP3 version 0.0\n"
            "ITBA Protocolos de Comunicación 2024Q2 -- Grupo 1\n"
            "Licencia ITBA\n"
  );
}

static void usage(const char* progname) {
  fprintf(
    stderr,
    "Usage: %s [OPTION]...\n"
    "\n"
    "   -h               Imprime la ayuda y termina.\n"
    "   -l <POP3 addr>   Dirección donde servirá el servidor POP.\n"
    "   -L <conf  addr>  Dirección donde servirá el servicio de management.\n"
    "   -p <POP3 port>   Puerto entrante conexiones POP3.\n"
    "   -P <conf port>   Puerto entrante conexiones configuracion\n"
    "   -u <name>:<pass> Usuario y contraseña de usuario que puede usar el servidor. Hasta 10.\n"
    "   -v               Imprime información sobre la versión versión y termina.\n"
    "   -d <dir>         Carpeta donde residen los Maildirs"
    "\n",
    progname
  );
  exit(1);
}

void parseArgs(const int argc, char** argv, pop3args* args) {
  memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

  args->socksAddr = "0.0.0.0";
  args->socksPort = 1080;

  args->mngAddr = "127.0.0.1";
  args->mngPort = 8080;

  args->doh.host = "localhost";
  args->doh.ip = "127.0.0.1";
  args->doh.port = 8053;
  args->doh.path = "/getnsrecord";
  args->doh.query = "?dns=";

  args->maildirPath = NULL;

  int c;
  int nusers = 0;

  while (true) {
    int option_index = 0;
    static struct option long_options[] = {
      {"doh-ip", required_argument, 0, 0xD001},    {"doh-port", required_argument, 0, 0xD002},
      {"doh-host", required_argument, 0, 0xD003},  {"doh-path", required_argument, 0, 0xD004},
      {"doh-query", required_argument, 0, 0xD005}, {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, "hl:L:Np:P:u:vd:", long_options, &option_index);
    if (c == -1) break;

    switch (c) {
    case 'h':
      usage(argv[0]);
      break;
    case 'l':
      args->socksAddr = optarg;
      break;
    case 'L':
      args->mngAddr = optarg;
      break;
    case 'p':
      args->socksPort = port(optarg);
      break;
    case 'P':
      args->mngPort = port(optarg);
      break;
    case 'u':
      if (nusers >= MAX_USERS) {
        fprintf(stderr, "maximun number of command line users reached: %d.\n", MAX_USERS);
        exit(1);
      } else {
        args->users[nusers] = newUser(optarg);
        nusers++;
      }
      break;
    case 'v':
      version();
      exit(0);
      break;
    case 'd':
      args->maildirPath = validatedDirectory(optarg);
      break;
    case 0xD001:
      args->doh.ip = optarg;
      break;
    case 0xD002:
      args->doh.port = port(optarg);
      break;
    case 0xD003:
      args->doh.host = optarg;
      break;
    case 0xD004:
      args->doh.path = optarg;
      break;
    case 0xD005:
      args->doh.query = optarg;
      break;
    default:
      fprintf(stderr, "unknown argument %d.\n", c);
      exit(1);
    }
  }
  for (int i = 0; i < nusers; ++i) {
    printf("user: %s, pass: %s\n", args->users[i].name, args->users[i].pass);
  }
  if (args->maildirPath == NULL) {
    fprintf(stderr, "Missing required flag `-d <maildir path>`\n");
    exit(EXIT_FAILURE);
  }
  if (optind < argc) {
    fprintf(stderr, "argument not accepted: ");
    while (optind < argc) {
      fprintf(stderr, "%s ", argv[optind++]);
    }
    fprintf(stderr, "\n");
    exit(1);
  }
  args->userCount = nusers;
}
