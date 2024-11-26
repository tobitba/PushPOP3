#ifndef COMMANDS_H
#define COMMANDS_H

#include "push3.h"

typedef struct PushCommandCDT* PushCommand;

PushCommand getPushCommand(buffer* b, const push3_state current);

push3_state runPushCommand(PushCommand command, push3* data);

#endif
