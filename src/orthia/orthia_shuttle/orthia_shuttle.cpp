#include "orthia_shuttle_interface.h"

ORTHIA_SHUTTLE_EXPORT(ping)
{
    orthia_shuttle::CPrintStream hyperOutput(arguments.pHypervisorInterface);
    hyperOutput<<"Hello, world! Arguments count: "<<arguments.argsCount<<"\n";
    for(int i = 0; i < arguments.argsCount; ++i)
    {
        hyperOutput<<" - "<<arguments.GetArgument(i)<<"\n";
    }
}



