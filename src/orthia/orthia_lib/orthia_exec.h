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

// The return addresses of a thread from its registers, walked by the x86 emulator in the memory of pMemoryReader.
// The 64-bit walk needs only RIP and RSP, the 32-bit one EBP
int QueryThreadCallStack(orthia::IMemoryReader * pMemoryReader,
                         int mode,
                         unsigned int pageSize,
                         const _Diana_Processor_Registers_Context * pContext,
                         int framesCount,
                         std::vector<OPERAND_SIZE> * pCallStack);

}

#endif
