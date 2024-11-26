/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en éste hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes (resolución de
 * DNS utilizando getaddrinfo), pero toda esa complejidad está oculta en
 * el selector.
 */
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h> // socket
#include <sys/types.h>  // socket
#include <unistd.h>

#include "include/args.h"
#include "include/authenticator.h"
#include "include/pop3.h"
#include "include/push3.h"
#include "include/selector.h"

static bool done = false;
pop3args args;

static void sigtermHandler(const int signal) {
  printf("signal %d, cleaning up and exiting\n", signal);
  done = true;
}

int main(int argc, char** argv) {

  parseArgs(argc, argv, &args);
  initAuthenticator(args.users, args.userCount);

  unsigned pop3_port = 2252;
  unsigned configurator_port = 2254;


  // no tenemos nada que leer de stdin
  close(0);

  const char* err_msg = NULL;
  selector_status ss = SELECTOR_SUCCESS;
  fd_selector selector = NULL;

  struct sockaddr_in addrPop; // OJO esto es solo ipv4
  memset(&addrPop, 0, sizeof(addrPop));
  addrPop.sin_family = AF_INET;
  addrPop.sin_addr.s_addr = htonl(INADDR_ANY);
  addrPop.sin_port = htons(pop3_port);

  struct sockaddr_in addrPush; // OJO esto es solo ipv4
  memset(&addrPush, 0, sizeof(addrPush));
  addrPush.sin_family = AF_INET;
  addrPush.sin_addr.s_addr = htonl(INADDR_ANY);
  addrPush.sin_port = htons(configurator_port);

  const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server < 0) {
    err_msg = "unable to create socket";
    goto finally;
  }
 

  const int pushServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (pushServer < 0) {
    err_msg = "unable to create socket";
    goto finally;
  }

  // man 7 ip. no importa reportar nada si falla.
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
  setsockopt(pushServer, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

  if (bind(server, (struct sockaddr*)&addrPop, sizeof(addrPop)) < 0) {
    err_msg = "unable to bind socket";
    goto finally;
  }

  if (bind(pushServer, (struct sockaddr*)&addrPush, sizeof(addrPush)) < 0) {
    err_msg = "unable to bind socket";
    goto finally;
  }

  if (listen(server, 20) < 0) {
    err_msg = "unable to listen";
    goto finally;
  }

  if (listen(pushServer, 20) < 0) {
    err_msg = "unable to listen";
    goto finally;
  }

   fprintf(stdout, "Listening for POP3 on TCP port %d\n", pop3_port);
   fprintf(stdout, "Listening for config on TCP port %d\n", configurator_port);

  // registrar sigterm es útil para terminar el programa normalmente.
  // esto ayuda mucho en herramientas como valgrind.
  signal(SIGTERM, sigtermHandler);
  signal(SIGINT, sigtermHandler);

  if (selector_fd_set_nio(server) == -1) {
    err_msg = "getting server socket flags";
    goto finally;
  }
  const struct selector_init conf = {
    .signal = SIGALRM,
    .select_timeout =
      {
        .tv_sec = 10,
        .tv_nsec = 0,
      },
  };
  if (0 != selector_init(&conf)) {
    err_msg = "initializing selector";
    goto finally;
  }

  selector = selector_new(1024);
  if (selector == NULL) {
    err_msg = "unable to create selector";
    goto finally;
  }
  const struct fd_handler pop3_pasive_handler = {
    .handle_read = pop3_passive_accept,
    .handle_write = NULL,
    .handle_close = NULL, // nada que liberar
  };
  ss = selector_register(selector, server, &pop3_pasive_handler, OP_READ, NULL);
  if (ss != SELECTOR_SUCCESS) {
    err_msg = "registering fd";
    goto finally;
  }


  const struct fd_handler push3_passive_handler = {
    .handle_read = push3_passive_accept,
    .handle_write = NULL,
    .handle_close = NULL, // nada que liberar
  };
  ss = selector_register(selector, pushServer, &push3_passive_handler, OP_READ, NULL);
  if (ss != SELECTOR_SUCCESS) {
    err_msg = "registering fd nuevo";
    goto finally;
  }

  while (!done) {
    err_msg = NULL;
    ss = selector_select(selector);
    if (ss != SELECTOR_SUCCESS) {
      err_msg = "serving";
      goto finally;
    }
  }
  if (err_msg == NULL) {
    err_msg = "closing";
  }

  int ret = 0;
finally:
  if (ss != SELECTOR_SUCCESS) {
    fprintf(
      stderr, "%s: %s\n", (err_msg == NULL) ? "" : err_msg, ss == SELECTOR_IO ? strerror(errno) : selector_error(ss)
    );
    ret = 2;
  } else if (err_msg) {
    perror(err_msg);
    ret = 1;
  }
  if (selector != NULL) {
    selector_destroy(selector);
  }
  selector_close();

  // socksv5_pool_destroy();

  if (server >= 0) {
    close(server);
  }
  return ret;
}

