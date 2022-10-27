#ifndef DIANA_STREAMS
#define DIANA_STREAMS

#include "diana_core.h"

typedef struct _dianaMemoryStream
{
    DianaReadWriteRandomStream parent;
    void * pBuffer;
    DIANA_SIZE_T bufferSize;
    DIANA_SIZE_T curSize;
}DianaMemoryStream;

void Diana_InitMemoryStream(DianaMemoryStream * pStream,
                            void * pBuffer,
                            DIANA_SIZE_T bufferSize);  // this

void Diana_InitMemoryStreamEx(DianaMemoryStream * pStream,
                              void * pBuffer,
                              DIANA_SIZE_T bufferSize,
                              int bWritable);  // this

int Diana_ParseCmdOnBuffer(int iMode,
                           void * pBuffer,
                           DIANA_SIZE_T size,
                           DianaBaseGenObject_type * pInitialLine,  // IN
                           DianaParserResult * pResult,  //OUT
                           DIANA_SIZE_T * sizeRead);    // OUT

int Diana_ParseCmdOnBuffer_testmode(int iMode,
                                    void * pBuffer,
                                    DIANA_SIZE_T size,
                                    DianaBaseGenObject_type * pInitialLine,  // IN
                                    DianaParserResult * pResult,  //OUT
                                    DIANA_SIZE_T * sizeRead);    // OUT

int DianaMemoryStream_RandomRead(void * pThis, 
                                   OPERAND_SIZE offset,
                                   void * pBuffer, 
                                   int iBufferSize, 
                                   OPERAND_SIZE * readBytes);

// common functions
int DianaStreams_CopyData(DianaReadStream * pStream,
                          DianaReadWriteRandomStream * pOutStream,
                          OPERAND_SIZE address,
                          OPERAND_SIZE size,
                          void * pPage,
                          int pageSize);
            
int DianaStreams_MemsetData(DianaReadWriteRandomStream * pOutStream,
                            OPERAND_SIZE address,
                            OPERAND_SIZE size,
                            int value,
                            void * pPage,
                            int pageSize);


#endif