#ifndef PUSH3_H
#define PUSH3_H

#include "args.h"
#include "buffer.h"
#include "maildir.h"
#include "selector.h"
#include "stm.h"

#define BUFFER_PUSH_SIZE 2096 // TODO : revisar tamaño

typedef enum push3_state {
    PUSH_GREETING,
    PUSH_AUTHORIZATION,
    PUSH_TRANSACTION,
    PUSH_ANYWHERE,
    PUSH_DONE,
    PUSH_ERROR,
    PUSH_FINISH,
} push3_state;

typedef struct push3 {
    struct state_machine stm;
    uint8_t writeData[BUFFER_PUSH_SIZE], readData[BUFFER_PUSH_SIZE];
    buffer *writeBuff, *readBuff;
    User user;
} push3;

void push3_passive_accept(struct selector_key* key);

#endif