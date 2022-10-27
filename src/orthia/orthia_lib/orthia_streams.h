#ifndef ORTHIA_STREAMS_H
#define ORTHIA_STREAMS_H

#include "diana_core_cpp.h"
#include "orthia_interfaces.h"

extern "C"
{
#include "diana_processor/diana_processor_streams.h"
}

namespace orthia
{


struct DianaMemoryStream:public DianaReadWriteRandomStream
{
    IMemoryReader * m_pMemoryReader;
    OPERAND_SIZE m_currentOffset;
    OPERAND_SIZE m_moduleSize;
    IMemoryReaderWriter * m_pMemoryReaderWriter;

    DianaMemoryStream(Address_type currentOffset, 
                      IMemoryReader * pMemoryReader,
                      OPERAND_SIZE moduleSize);
    DianaMemoryStream(Address_type currentOffset, 
                      IMemoryReaderWriter * pMemoryReaderWriter,
                      OPERAND_SIZE moduleSize);

};


int DianaAnalyzeMoveTo(void * pThis, 
                       OPERAND_SIZE offset);
int DianaRead(void * pThis, 
              void * pBuffer, 
              int iBufferSize, 
              int * readed);
int DianaRandomRead(void * pThis, 
                       OPERAND_SIZE offset,
                       void * pBuffer, 
                       int iBufferSize, 
                       OPERAND_SIZE * readBytes,
                       int flags);
int DianaRandomWrite(void * pThis, 
                       OPERAND_SIZE offset,
                       void * pBuffer, 
                       int iBufferSize, 
                       OPERAND_SIZE * readBytes,
                       int flags);



struct ProcessorReadWriteStream:DianaRandomReadWriteStream
{
    IMemoryReaderWriter * m_pMemoryReaderWriter;

    ProcessorReadWriteStream(IMemoryReaderWriter * pMemoryReaderWriter);
};

struct DianaAnalyzerReadWriteStream:DianaReadWriteRandomStream
{
    IMemoryReaderWriter * m_pMemoryReaderWriter;
    OPERAND_SIZE m_currentPosition;

    DianaAnalyzerReadWriteStream(IMemoryReaderWriter * pMemoryReaderWriter);
};
bool IsDefaultSelector(DianaUnifiedRegister selector);
}
#endif

