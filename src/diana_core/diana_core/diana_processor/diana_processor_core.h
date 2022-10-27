#ifndef DIANA_PROCESSOR_CORE_H
#define DIANA_PROCESSOR_CORE_H


#include "diana_processor_core_impl.h"
#include "diana_processor_core_impl_xmm.h"
#include "diana_core.h"


// call it after Diana_Init
void DianaProcessor_GlobalInit();

int DianaProcessor_Init(DianaProcessor * pThis, 
                        DianaRandomReadWriteStream * pMemoryStream,
                        Diana_Allocator * pAllocator,
                        int mode);

void DianaProcessor_Free(DianaProcessor * pThis);

int DianaProcessor_ExecOnce(DianaProcessor * pThis);

void DianaProcessor_ClearCache(DianaProcessor * pThis);

void DianaProcessor_InitRawRegister(DianaProcessor * pCallContext,
                                    DianaUnifiedRegister regId,
                                    const DI_CHAR * pValue,
                                    int size);
void DianaProcessor_ResetFPU(DianaProcessor * pThis);
const DI_CHAR * DianaProcessor_QueryRawRegister(DianaProcessor * pCallContext,
                                                DianaUnifiedRegister regId);

void DianaProcessor_InitCustomCommandProvider(DianaProcessor * pProc,
                                              DianaProcessorCustomCommandProvider_type customProvider,
                                               void * pCustomProviderContext);



typedef int (*DianaProcessorStackHandler_type)(struct _dianaProcessor * pProcessor,
                                               void * pCustomContext,
                                               OPERAND_SIZE retAddress,
                                               int * pbContinue);


int DianaProcessor_QueryCurrentStack(DianaProcessor * pProc,
                                      DianaProcessorStackHandler_type pObserver,
                                      void * pCustomContext);
#endif