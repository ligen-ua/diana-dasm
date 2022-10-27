#ifndef DIANA_PE_ANALYZER_H
#define DIANA_PE_ANALYZER_H

#include "diana_pe.h"
#include "diana_analyze.h"

typedef struct _DianaMovableReadStreamOverMemory
{
    DianaMovableReadStream stream;
    DianaMemoryStream memoryStream;
}DianaMovableReadStreamOverMemory;

typedef struct _DianaAnalyzeObserverOverMemory
{
    DianaAnalyzeObserver parent;
    DianaMovableReadStreamOverMemory stream;
}DianaAnalyzeObserverOverMemory;

void DianaAnalyzeObserverOverMemory_Init(DianaAnalyzeObserverOverMemory * pThis,
                                         void * pPeFile,
                                         OPERAND_SIZE fileSize);
void DianaMovableReadStreamOverMemory_Init(DianaMovableReadStreamOverMemory * pThis,
                                            const void * pBuffer,
                                            OPERAND_SIZE bufferSize);
int Diana_PE_AnalyzePE(Diana_PeFile * pPeFile,
                       DianaAnalyzeObserver * pObserver,
                       Diana_InstructionsOwner * pOwner,
                       int analyserFlags);

int Diana_PE_AnalyzePEInMemory(void * pPeFile,
                               OPERAND_SIZE fileSize,
                               Diana_InstructionsOwner * pOwner,
                               int analyserFlags);

#endif