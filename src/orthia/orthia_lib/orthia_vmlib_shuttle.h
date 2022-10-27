#ifndef ORTHIA_VMLIB_SHUTTLE_H
#define ORTHIA_VMLIB_SHUTTLE_H

#include "orthia_interfaces.h"
#include "orthia_database_module.h"


namespace orthia
{


struct PrebuiltShuttleArgument
{
    std::vector<char> buffer;
    int trapStartOffset;
    int functionsCount;
    PrebuiltShuttleArgument()
        :
            trapStartOffset(0),
            functionsCount(0)
    {
    }
};
class CShuttleArgsBuilder
{
    int m_dianaMode;
    int m_argsCountOffset;
    int m_trapStartOffset;
    std::vector<char> m_buffer;
public:
    CShuttleArgsBuilder(Address_type offset, int dianaMode);
    void AddArgument(Address_type arg);
    void Produce(PrebuiltShuttleArgument * pArg);
};

class CShuttleAPIHandlerPopulator
{
public:
    CShuttleAPIHandlerPopulator();
    void RegisterHandlers(ICommonAPIHandlerStorage * pCommonAPIHandlerStorage,
                          Address_type argumentsOffset);
};


}


#endif
