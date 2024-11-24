#ifndef POP3_H
#define POP3_H
#include <netdb.h>
#include "selector.h"
#include "buffer.h"
#include "stm.h"

#define BUFFER_SIZE 521  //TODO : revisar tamaño


enum pop3_states
{
    // For USER, PASS, CAPA and QUIT commands
    AUTHORIZATION,
    // For bla bla commands
    TRANSACTION,
    // MUst delete all dell mails and quit
    UPDATE,
    // If needed, add more states here

    // Final states
    ERROR,
    FINISH, 
};


typedef struct pop3
{
    struct state_machine stm;
    uint8_t raw_buff[BUFFER_SIZE];
    buffer *buff;

} pop3;


void pop3_passive_accept(struct selector_key *key);



#endif