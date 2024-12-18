#include "../include/push3.h"
#include "../include/buffer.h"
#include "../include/pushCommands.h"
#include "../include/selector.h"
#include "../include/stm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define ATTACHMENT(key) ((push3*)(key)->data)

static unsigned push_write(struct selector_key* key) {
  push3* data = key->data;
  size_t count;
  uint8_t* ptr = buffer_read_ptr(data->writeBuff, &count);

  const ssize_t n = send(key->fd, ptr, count, MSG_NOSIGNAL);
  if (n == -1) {
    int fd = key->fd;
    selector_unregister_fd(key->s, fd);
    close(fd);
    return 0;
  }
  if (n < 0) {
  } else {
    buffer_reset(data->writeBuff);
    selector_set_interest_key(key, OP_READ);
  }
  push3_state currentState = data->stm.current->state;
  if (currentState == PUSH_GREETING) {
    return PUSH_AUTHORIZATION;
  }
  if (currentState == PUSH_DONE || currentState == PUSH_ERROR) {
    return PUSH_FINISH;
  }
  return currentState;
}

static unsigned push_read(struct selector_key* key) {
  push3* data = ATTACHMENT(key);
  size_t count;
  uint8_t* ptr = buffer_write_ptr(data->readBuff, &count);
  ssize_t n = recv(key->fd, ptr, count, 0);
  if (n <= 0) {
    return PUSH_ERROR;
  }
  buffer_write_adv(data->readBuff, n);
  PushCommand command = getPushCommand(data->readBuff, data->stm.current->state);
  push3_state newState = runPushCommand(command, data);
  selector_set_interest_key(key, OP_WRITE);
  return newState;
}

void push_greeting(const unsigned state, struct selector_key* key) {
  size_t lenght;
  char greeting[] = "->SUCCESS: Welcome to Push3\r\n";
  uint8_t* buf = buffer_write_ptr(ATTACHMENT(key)->writeBuff, &lenght);
  memcpy(buf, greeting, strlen(greeting) + 1);
  buffer_write_adv(ATTACHMENT(key)->writeBuff, lenght);
}

static const struct state_definition push3_states_handlers[] = {
  {
    .state = PUSH_GREETING,
    .on_arrival = push_greeting,
    .on_write_ready = push_write,
  },
  {
    .state = PUSH_AUTHORIZATION,
    .on_read_ready = push_read,
    .on_write_ready = push_write,
  },
  { .state = PUSH_TRANSACTION,
    .on_read_ready = push_read,
    .on_write_ready = push_write,
  },
  {
    .state = PUSH_ANYWHERE
  },{
    .state = PUSH_DONE,
    .on_write_ready = push_write
  },
  {.state = PUSH_ERROR},
  {.state = PUSH_FINISH},
};

static void push3_done(struct selector_key* key) {
  int fd = key->fd;
  if (fd != -1) {
    if (SELECTOR_SUCCESS != selector_unregister_fd(key->s, fd)) {
      abort();
    }
    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = 0;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));

    close(fd);
  }
}

static void push3_read(struct selector_key* key) {
  struct state_machine* stm = &ATTACHMENT(key)->stm;
  const push3_state st = stm_handler_read(stm, key);

  if (PUSH_ERROR == st || PUSH_FINISH == st) {
    push3_done(key);
  }
}

static void push3_write(struct selector_key* key) {
  struct state_machine* stm = &ATTACHMENT(key)->stm;
  const push3_state st = stm_handler_write(stm, key);

  if (PUSH_ERROR == st || PUSH_FINISH == st) {
    push3_done(key);
  }
}

static void push3_block(struct selector_key* key) {
  struct state_machine* stm = &ATTACHMENT(key)->stm;
  const push3_state st = stm_handler_block(stm, key);

  if (PUSH_ERROR == st || PUSH_FINISH == st) {
    push3_done(key);
  }
}

static void push3_close(struct selector_key* key) {
  push3* toFree = ATTACHMENT(key);
  free(toFree->writeBuff);
  free(toFree->readBuff);
  free(toFree->user.name);
  free(toFree->user.pass);
  free(toFree);

  // socks5_destroy(ATTACHMENT(key)); TODO
}

static const struct fd_handler push3_handler = {
  .handle_read = push3_read,
  .handle_write = push3_write,
  .handle_close = push3_close,
  .handle_block = push3_block,
};

void push3_passive_accept(struct selector_key* key) {
  push3* data = NULL;
  const int client = accept(key->fd, NULL, NULL); // TODO: revisar argumentos
  if (client == -1) {
    goto fail;
  }
  if (selector_fd_set_nio(client) == -1) {
    goto fail;
  }
  data = calloc(1, sizeof(push3));
  data->readBuff = malloc(sizeof(struct buffer));
  buffer_init(data->readBuff, BUFFER_PUSH_SIZE, data->readData);
  data->writeBuff = malloc(sizeof(struct buffer));
  buffer_init(data->writeBuff, BUFFER_PUSH_SIZE, data->writeData);
  data->stm.initial = PUSH_GREETING;
  data->stm.max_state = PUSH_FINISH;
  data->stm.states = push3_states_handlers;
  stm_init(&data->stm);
  if (SELECTOR_SUCCESS != selector_register(key->s, client, &push3_handler, OP_WRITE, data)) {
    goto fail;
  }
  return;
fail:
  if (client != -1) {
    close(client);
  }
  // socks5_destroy(state); TODO: ver los frees y demas cosas no tan lindas...
}
