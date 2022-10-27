#ifndef ORTHIA_EXEC_H
#define ORTHIA_EXEC_H

#include "orthia_interfaces.h"
#include "diana_processor_cpp.h"
#include "orthia_memory_cache.h"
#include "orthia_streams.h"

struct _Diana_Processor_Registers_Context;

namespace orthia
{

class CProcessor:public diana::CBaseProcessor
{
    CMemoryStorageOfModifiedData m_cache;
    orthia::ProcessorReadWriteStream m_memoryStream;
    orthia::Ptr<IAPIHandler> m_pAPIHandler;
protected:
    void InitProcessor();
public:
    CProcessor(orthia::IMemoryReader * pMemoryReader,
               int mode,
               orthia::Ptr<IAPIHandler> pAPIHandler);
    int HandleAPI(OPERAND_SIZE rip,
                  bool * pApiHandled);
    CMemoryStorageOfModifiedData & GetCache();
};


int Exec(orthia::IMemoryReader * pMemoryReader,
          int mode,
          _Diana_Processor_Registers_Context * pContext,
          long long commandsCount,
          CMemoryStorageOfModifiedData & allWrites,
          IDebugger * pDebugger,
          long long * pCommandsCount,
          orthia::Ptr<IAPIHandler> pAPIHandler,
          std::vector<OPERAND_SIZE> * pCallStack = 0);


}

#endif
