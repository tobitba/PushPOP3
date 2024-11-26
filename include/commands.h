#ifndef COMMANDS_H
#define COMMANDS_H

#include "pop3.h"

typedef struct CommandCDT* Command;

Command getCommand(buffer* b, const state current);

state runCommand(Command command, pop3* data);
state continuePendingCommand(pop3* data);

#endif
