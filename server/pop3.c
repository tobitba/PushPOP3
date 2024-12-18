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

#include "../include/serverMetrics.h"

#define ATTACHMENT(key) ((pop3*)(key)->data)

static void pop3_done(struct selector_key* key);
static void pop3_close(struct selector_key* key);

static state pop_write(struct selector_key* key) {
  pop3* data = key->data;
  size_t count;
  uint8_t* ptr = buffer_read_ptr(data->writeBuff, &count);

  const ssize_t n = send(key->fd, ptr, count, MSG_NOSIGNAL);
  if (n < 0) {
    int fd = key->fd;
    selector_unregister_fd(key->s, fd);
    close(fd);
    return FINISH;
  } else {
    buffer_reset(data->writeBuff);
    selector_set_interest_key(key, OP_READ);
  }
  state currentState = data->stm.current->state;
  if (currentState == GREETING) {
    return AUTHORIZATION;
  }
  if (currentState == UPDATE || currentState == ERROR) {
    return FINISH;
  }
  return currentState;
}

static state pop_read(struct selector_key* key) {
  pop3* data = ATTACHMENT(key);
  size_t count;
  uint8_t* ptr = buffer_write_ptr(data->readBuff, &count);
  ssize_t n = recv(key->fd, ptr, count, 0);
  if (n <= 0) {
    return ERROR;
  }
  buffer_write_adv(data->readBuff, n);
  Command command = getCommand(data->readBuff, data->stm.current->state);
  state newState = runCommand(command, data);
  selector_set_interest_key(key, OP_WRITE);
  return newState;
}

void pop_greeting(const unsigned state, struct selector_key* key) {
  size_t lenght;
  uint8_t* buf = buffer_write_ptr(ATTACHMENT(key)->writeBuff, &lenght);
  char greeting[] = "+OK PushPop3\r\n";
  memcpy(buf, greeting, sizeof(greeting) - 1);
  buffer_write_adv(ATTACHMENT(key)->writeBuff, sizeof(greeting) - 1);
}

static state pending_write(struct selector_key* key) {
  pop3* data = key->data;
  size_t count;
  uint8_t* ptr = buffer_read_ptr(data->writeBuff, &count);

  const ssize_t n = send(key->fd, ptr, count, MSG_NOSIGNAL);
  if (n < 0) {
    int fd = key->fd;
    selector_unregister_fd(key->s, fd);
    close(fd);
    return 0;
  } else {
    buffer_reset(data->writeBuff);
  }
  return continuePendingCommand(data);
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
  {
    .state = UPDATE,
    .on_write_ready = pop_write,
  },
  {
    .state = PENDING_RESPONSE,
    .on_write_ready = pending_write,
  },
  {.state = ERROR},
  {
    .state = FINISH
  },
};

static void pop3_done(struct selector_key* key) {
  int fd = key->fd;
  if (fd != -1) {
    selector_status selStatus = selector_unregister_fd(key->s, fd);
    if (SELECTOR_SUCCESS != selStatus) {
      abort();
    }
    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = 0;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));

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
  decrementCurrentConnections();

  // socks5_destroy(ATTACHMENT(key)); TODO
}

static const struct fd_handler pop3_handler = {
  .handle_read = pop3_read,
  .handle_write = pop3_write,
  .handle_close = pop3_close,
  .handle_block = pop3_block,
};

void pop3_passive_accept(struct selector_key* key) {
  pop3* data = NULL;
  const int client = accept(key->fd, NULL, NULL); // TODO: revisar argumentos
  if (client == -1 || selector_fd_set_nio(client) == -1) goto fail;

  data = calloc(1, sizeof(pop3));
  data->readBuff = malloc(sizeof(struct buffer));
  buffer_init(data->readBuff, BUFFER_SIZE, data->readData);
  data->writeBuff = malloc(sizeof(struct buffer));
  buffer_init(data->writeBuff, BUFFER_SIZE, data->writeData);
  data->stm.initial = GREETING;
  data->stm.max_state = FINISH;
  data->stm.states = pop3_states_handlers;
  stm_init(&data->stm);
  data->mails = NULL;

  if (SELECTOR_SUCCESS != selector_register(key->s, client, &pop3_handler, OP_WRITE, data)) {
    goto fail;
  }
  incrementCurrentConnections();
  incrementTotalConnections();
  return;
fail:
  if (client != -1) {
    close(client);
  }
  // socks5_destroy(state); TODO: ver los frees y demas cosas no tan lindas...
}