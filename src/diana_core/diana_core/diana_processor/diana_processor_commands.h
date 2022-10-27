#ifndef DIANA_PROCESSOR_COMMANDS_H
#define DIANA_PROCESSOR_COMMANDS_H

#define DI_PROC_REGISTER_COMMAND_EX(Name, Command) \
    case diana_cmd_ ## Name: \
        DIANA_SET_PROCESSOR_TAG(p, Command); \
        break;

#define DI_PROC_REGISTER_COMMAND(Name) \
    case diana_cmd_ ## Name: \
        DIANA_SET_PROCESSOR_TAG(p, Diana_Call_ ## Name); \
        break;
//---

void DianaProcessor_LinkCommands();

#endif
