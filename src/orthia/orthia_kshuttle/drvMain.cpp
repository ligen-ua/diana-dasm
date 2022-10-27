extern "C"
{
#include "ntifs.h"
}

#include "orthia_shuttle_interface.h"



ORTHIA_SHUTTLE_EXPORT2(ping)
{
    #pragma ORTHIA_FORCE_EXPORT

    orthia_shuttle::CPrintStream hyperOutput(arguments.pHypervisorInterface);
    hyperOutput<<"Hello from orthia_kshuttle. Arguments count: "<<arguments.argsCount<<"\n";
    for(int i = 0; i < arguments.argsCount; ++i)
    {
        hyperOutput<<" - "<<arguments.GetArgument(i)<<"\n";
    }
}


// DriverEntry
extern "C"
NTSTATUS
DriverEntry(IN PDRIVER_OBJECT   pDriverObject,
            IN PUNICODE_STRING  RegistryPath
)
{
    return STATUS_SUCCESS;
}



