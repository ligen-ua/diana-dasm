#ifndef DIANA_PROCESSOR_STREAMS_H
#define DIANA_PROCESSOR_STREAMS_H

#include "diana_streams.h"

// forward declaration
struct _dianaProcessor;

// random stream
typedef int (* DianaRandomRead_fnc)(void * pThis, 
                                    OPERAND_SIZE selector,
                                    OPERAND_SIZE offset,
                                    void * pBuffer, 
                                    OPERAND_SIZE iBufferSize, 
                                    OPERAND_SIZE * readed,
                                    struct _dianaProcessor * pProcessor,
                                    DianaUnifiedRegister segReg);
typedef int (* DianaRandomWrite_fnc)(void * pThis, 
                                     OPERAND_SIZE selector,
                                     OPERAND_SIZE offset,
                                     void * pBuffer, 
                                     OPERAND_SIZE iBufferSize, 
                                     OPERAND_SIZE * wrote,
                                     struct _dianaProcessor * pProcessor,
                                     DianaUnifiedRegister segReg);


typedef struct _dianaRandomReadWriteStream
{
    DianaRandomRead_fnc pReadFnc;
    DianaRandomWrite_fnc pWriteFnc;
}DianaRandomReadWriteStream;

typedef struct _dianaRandomMemoryStream
{
    DianaRandomReadWriteStream parent;
    void * pBuffer;
    OPERAND_SIZE bufferSize;
    OPERAND_SIZE segmentSize;
}DianaRandomMemoryStream;

void Diana_InitRandomMemoryStream(DianaRandomMemoryStream * pStream,
                                  void * pBuffer,
                                  OPERAND_SIZE bufferSize,
                                  OPERAND_SIZE segmentSize
                                  );  


#endif