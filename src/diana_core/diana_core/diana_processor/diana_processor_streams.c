#include "diana_processor_streams.h"



static int MemoryRandomStream_ReadWrite(void * pThis, 
                                        OPERAND_SIZE selector,
                                        OPERAND_SIZE offset_in,
                                        void * pBuffer, 
                                        OPERAND_SIZE iBufferSize, 
                                        OPERAND_SIZE * readed,
                                        int bWrite,
                                        struct _dianaProcessor * pProcessor)
{
    DianaRandomMemoryStream * pStream = pThis;
    OPERAND_SIZE sizeToGive = 0;
    OPERAND_SIZE segmentOffset = 0;
    OPERAND_SIZE offset = 0;

    pProcessor;

    segmentOffset = selector*pStream->segmentSize;
    offset = segmentOffset + offset_in;

    *readed = 0;
    if (offset > pStream->bufferSize)
        return DI_ERROR;

    sizeToGive = pStream->bufferSize - offset;
    if (sizeToGive > iBufferSize)
        sizeToGive = iBufferSize;

    if (bWrite)
    {
        DIANA_MEMCPY((char*)pStream->pBuffer+offset,
               pBuffer,
               (DIANA_SIZE_T)sizeToGive);
    }
    else
    {
        DIANA_MEMCPY(pBuffer,
            (char*)pStream->pBuffer+offset, 
            (DIANA_SIZE_T)sizeToGive);
    }

    *readed = sizeToGive;
    return DI_SUCCESS;
}

static int MemoryRandomStream_Read(void * pThis, 
                                   OPERAND_SIZE selector,
                                   OPERAND_SIZE offset,
                                   void * pBuffer, 
                                   OPERAND_SIZE iBufferSize, 
                                   OPERAND_SIZE * readed,
                                   struct _dianaProcessor * pProcessor,
                                   DianaUnifiedRegister segReg)
{
    &segReg;
    return MemoryRandomStream_ReadWrite(pThis, 
                                        selector,
                                        offset,
                                        pBuffer, 
                                        iBufferSize, 
                                        readed,
                                        0,
                                        pProcessor);
}

static int MemoryRandomStream_Write(void * pThis, 
                                    OPERAND_SIZE selector,
                                    OPERAND_SIZE offset,
                                    void * pBuffer, 
                                    OPERAND_SIZE iBufferSize, 
                                    OPERAND_SIZE * readed,
                                    struct _dianaProcessor * pProcessor,
                                    DianaUnifiedRegister segReg)
 {
    &segReg;
    return MemoryRandomStream_ReadWrite(pThis, 
                                        selector,
                                        offset,
                                        pBuffer, 
                                        iBufferSize, 
                                        readed,
                                        1,
                                        pProcessor);
}

void Diana_InitRandomMemoryStream(DianaRandomMemoryStream * pStream,
                                  void * pBuffer,
                                  OPERAND_SIZE bufferSize,
                                  OPERAND_SIZE segmentSize
                                  )
{
    pStream->pBuffer = pBuffer;
    pStream->bufferSize = bufferSize;
    pStream->segmentSize = segmentSize;
    pStream->parent.pReadFnc = MemoryRandomStream_Read;
    pStream->parent.pWriteFnc = MemoryRandomStream_Write;
}
