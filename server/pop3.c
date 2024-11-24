#include "../include/pop3.h"
#include <stdio.h>
#include <stdlib.h>
#include "../include/selector.h"

#define ATTACHMENT(key) ( (pop3*)(key)->data)

static const struct state_definition pop3_states_handlers[] = {
    {
        .state = AUTHORIZATION,
        .on_arrival = NULL,        
        .on_read_ready = NULL,  
        .on_write_ready = NULL, 
    },
    {
        .state = TRANSACTION
    },
    {
        .state = UPDATE
    },
    {
        .state = ERROR
    },
    {
        .state = FINISH
    },
};

static void
pop3_done(struct selector_key* key) {
    int fd = key->fd;
    if(fd != -1) {
        if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fd)) {
            abort();
        }
        close(fd);
    }
    
}

static void
pop3_read(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_states st = stm_handler_read(stm, key);

    if(ERROR == st || FINISH == st) {
        pop3_done(key);
    }
}


static void
pop3_write(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_states st = stm_handler_write(stm, key);

    if(ERROR == st || FINISH == st) {
        pop3_done(key);
    }
}

static void
pop3_block(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_states st = stm_handler_block(stm, key);

    if(ERROR == st || FINISH == st) {
        pop3_done(key);
    }
}

static void
pop3_close(struct selector_key *key) {
    //socks5_destroy(ATTACHMENT(key)); TODO
}





static const struct fd_handler pop3_handler = {
    .handle_read = pop3_read,
    .handle_write = pop3_write,
    .handle_close = pop3_close,
    .handle_block = pop3_block,
};



void pop3_passive_accept(struct selector_key *key){
    pop3 *datos = NULL;
    const int client = accept(key->fd, NULL, NULL); //TODO: revisar argumentos
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    datos = malloc(sizeof(pop3));
    datos->buff = malloc(sizeof(struct buffer));
    buffer_init(datos->buff,BUFFER_SIZE,datos->raw_buff);
    datos->stm.initial = AUTHORIZATION;
    datos->stm.max_state = FINISH;
    datos->stm.states = pop3_states_handlers;
    stm_init(&datos->stm);


    if(SELECTOR_SUCCESS != selector_register(key->s, client, &pop3_handler,
                                              OP_WRITE, datos)) {
        goto fail;
                                              }
    return ;
    fail:
        if(client != -1) {
            close(client);
        }
   // socks5_destroy(state); TODO: ver los frees y demas cosas no tan lindas...
}