#ifndef POP3_H
#define POP3_H
#include <netdb.h>
#include "selector.h"
#include "buffer.h"
#include "stm.h"
#include "args.h"

#define BUFFER_SIZE 521  //TODO : revisar tamaño


enum pop3_states
{
    // For USER,  CAPA and QUIT commands
    AUTHORIZATION,
    //For PASS and QUIT
    AUTHORIZATION_PASS,
    // For bla bla commands
    TRANSACTION,
    // MUst delete all dell mails and quit
    UPDATE,
    // If needed, add more states here

    // Final states
    ERROR,
    FINISH, 
};

typedef enum pop3_states state;

typedef struct pop3
{
    struct state_machine stm;
    uint8_t writeData[BUFFER_SIZE], readData[BUFFER_SIZE];
    buffer *writeBuff, *readBuff;
    struct users user;

} pop3;


void pop3_passive_accept(struct selector_key *key);


#endif