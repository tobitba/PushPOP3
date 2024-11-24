#ifndef COMMANDS_H
#define COMMANDS_H

#include "pop3.h"

typedef state (*handler) (pop3 * datos, char* arg1, char* arg2);

typedef struct CommandCDT * Command;

Command getCommand(buffer *b, const state current);

state runCommand(Command command, pop3* datos);

#endif
