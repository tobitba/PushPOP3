#ifndef COMMANDS_H
#define COMMANDS_H

#include "pop3.h"

// We could add more argument if needed, and maybe mix arg1 and the bool in an struct.
typedef state (*handler)(pop3* data, char* arg, bool isArgPresent);

typedef struct CommandCDT* Command;

Command getCommand(buffer* b, const state current);

state runCommand(Command command, pop3* data);

#endif
