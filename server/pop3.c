#include "../include/pop3.h"
#include "../include/buffer.h"
#include "../include/commands.h"
#include "../include/maildir.h"
#include "../include/selector.h"
#include "../include/stm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define ATTACHMENT(key) ((pop3*)(key)->data)

static unsigned pop_write(struct selector_key* key) {
  pop3* datos = key->data;
  size_t count;
  uint8_t* ptr = buffer_read_ptr(datos->writeBuff, &count);

  const ssize_t n = send(key->fd, ptr, count, MSG_NOSIGNAL);
  if (n == -1) {
    int fd = key->fd;
    selector_unregister_fd(key->s, fd);
    close(fd);
    return 0;
  }
  if (n < 0) {
    printf("no pude mandar datos : (");
  } else {
    buffer_reset(datos->writeBuff);
    selector_set_interest_key(key, OP_READ);
  }
  state currentState = datos->stm.current->state;
  if (currentState == GREETING) {
    return AUTHORIZATION;
  }
  return currentState;
}

static unsigned pop_read(struct selector_key* key) {
  pop3* datos = ATTACHMENT(key);
  size_t count;
  uint8_t* ptr = buffer_write_ptr(datos->readBuff, &count);
  ssize_t n = recv(key->fd, ptr, count, 0);
  printf("read\n");
  if (n <= 0) {
    return ERROR;
  } else {
    buffer_write_adv(datos->readBuff, n);
    Command command = getCommand(datos->readBuff, datos->stm.current->state);
    state newState = runCommand(command, datos);
    printf("nuevo estado: %d\n", newState);
    selector_set_interest_key(key, OP_WRITE);
    return newState;
  }

  return datos->stm.current->state;
}

void pop_greeting(const unsigned state, struct selector_key* key) {
  size_t lenght;
  char greeting[] = "+OK PushPop3\r\n";
  uint8_t* buf = buffer_write_ptr(ATTACHMENT(key)->writeBuff, &lenght);
  memcpy(buf, greeting, strlen(greeting) + 1);
  buffer_write_adv(ATTACHMENT(key)->writeBuff, lenght);
}

static const struct state_definition pop3_states_handlers[] = {
  {
    .state = GREETING,
    .on_arrival = pop_greeting,
    .on_write_ready = pop_write,
  },
  {
    .state = AUTHORIZATION,
    .on_read_ready = pop_read,
    .on_write_ready = pop_write,
  },
  {
    .state = AUTHORIZATION_PASS,
    .on_read_ready = pop_read,
    .on_write_ready = pop_write,
  },
  {
    .state = TRANSACTION,
    .on_write_ready = pop_write,
    .on_read_ready = pop_read,

  },
  {.state = ANYWHERE},
  {.state = UPDATE},
  {.state = ERROR},
  {.state = FINISH},
};

static void pop3_done(struct selector_key* key) {
  int fd = key->fd;
  if (fd != -1) {
    if (SELECTOR_SUCCESS != selector_unregister_fd(key->s, fd)) {
      abort();
    }
    close(fd);
  }
}

static void pop3_read(struct selector_key* key) {
  struct state_machine* stm = &ATTACHMENT(key)->stm;
  const enum pop3_states st = stm_handler_read(stm, key);

  if (ERROR == st || FINISH == st) {
    pop3_done(key);
  }
}

static void pop3_write(struct selector_key* key) {
  printf("write handleeer\n");
  struct state_machine* stm = &ATTACHMENT(key)->stm;
  const enum pop3_states st = stm_handler_write(stm, key);

  if (ERROR == st || FINISH == st) {
    pop3_done(key);
  }
}

static void pop3_block(struct selector_key* key) {
  struct state_machine* stm = &ATTACHMENT(key)->stm;
  const enum pop3_states st = stm_handler_block(stm, key);

  if (ERROR == st || FINISH == st) {
    pop3_done(key);
  }
}

static void pop3_close(struct selector_key* key) {
  pop3* toFree = ATTACHMENT(key);
  free(toFree->writeBuff);
  free(toFree->readBuff);
  free(toFree->user.name);
  free(toFree->user.pass);
  if (toFree->mails != NULL) maildirFree(toFree->mails);
  free(toFree);

  // socks5_destroy(ATTACHMENT(key)); TODO
}

static const struct fd_handler pop3_handler = {
  .handle_read = pop3_read,
  .handle_write = pop3_write,
  .handle_close = pop3_close,
  .handle_block = pop3_block,
};

void pop3_passive_accept(struct selector_key* key) {
  fprintf(stdout, "pasive handler\n");
  pop3* datos = NULL;
  const int client = accept(key->fd, NULL, NULL); // TODO: revisar argumentos
  if (client == -1) {
    goto fail;
  }
  if (selector_fd_set_nio(client) == -1) {
    goto fail;
  }
  datos = calloc(1, sizeof(pop3));
  datos->readBuff = malloc(sizeof(struct buffer));
  buffer_init(datos->readBuff, BUFFER_SIZE, datos->readData);
  datos->writeBuff = malloc(sizeof(struct buffer));
  buffer_init(datos->writeBuff, BUFFER_SIZE, datos->writeData);
  datos->stm.initial = GREETING;
  datos->stm.max_state = FINISH;
  datos->stm.states = pop3_states_handlers;
  stm_init(&datos->stm);
  printf("agrego a selector\n");
  datos->mails = NULL;

  if (SELECTOR_SUCCESS != selector_register(key->s, client, &pop3_handler, OP_WRITE, datos)) {
    goto fail;
  }
  printf("agregado a selector\n");
  return;
fail:
  if (client != -1) {
    close(client);
  }
  // socks5_destroy(state); TODO: ver los frees y demas cosas no tan lindas...
}
