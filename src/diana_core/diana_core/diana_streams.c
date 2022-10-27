#include "diana_streams.h"

static int MemoryStream_Read(void * pThis, void * pBuffer, int iBufferSize, int * readBytes)
{
    DianaMemoryStream * pStream = pThis;
    int sizeToGive = (int)(pStream->bufferSize - pStream->curSize);
    if (sizeToGive > iBufferSize)
        sizeToGive = iBufferSize;

    DIANA_MEMCPY(pBuffer,(char*)pStream->pBuffer+pStream->curSize, sizeToGive);
    pStream->curSize += sizeToGive;
    *readBytes = sizeToGive;
    return 0;
}

int DianaMemoryStream_RandomRead(void * pThis, 
                                   OPERAND_SIZE offset,
                                   void * pBuffer, 
                                   int iBufferSize, 
                                   OPERAND_SIZE * readBytes)
{
    DianaMemoryStream * pStream = pThis;
    int sizeToGive = 0;
    
    if (offset >= pStream->bufferSize)
    {
        return DI_END_OF_STREAM;
    }

    sizeToGive = (int)(pStream->bufferSize - offset);
    if (sizeToGive > iBufferSize)
        sizeToGive = iBufferSize;

    DIANA_MEMCPY(pBuffer,(char*)pStream->pBuffer+offset, sizeToGive);
    *readBytes = sizeToGive;
    return 0;
}
int DianaMemoryStream_RandomReadMoveTo(void * pThis, 
                                       OPERAND_SIZE offset)
{
    DianaMemoryStream * pStream = (DianaMemoryStream * )pThis;
    pStream->curSize = (DIANA_SIZE_T)offset;
    return DI_SUCCESS;
}
int DianaMemoryStream_RandomWrite_V(void * pThis, 
                                   OPERAND_SIZE offset,
                                   void * pBuffer, 
                                   int iBufferSize, 
                                   OPERAND_SIZE * readBytes,
                                   int flags)
{
    DianaMemoryStream * pStream = (DianaMemoryStream * )pThis;
    OPERAND_SIZE sizeToGive = 0;
    
    if (flags & DIANA_ANALYZE_RANDOM_READ_ABSOLUTE)
    {
        return DI_END_OF_STREAM;
    }
    if (offset >= pStream->bufferSize)
    {
        return DI_END_OF_STREAM;
    }

    sizeToGive = (int)(pStream->bufferSize - offset);
    if (sizeToGive > iBufferSize)
        sizeToGive = iBufferSize;

    DIANA_MEMCPY((char*)pStream->pBuffer+offset, pBuffer, (DIANA_SIZE_T)sizeToGive);
    *readBytes = sizeToGive;
    return 0;
}
                                  
int DianaMemoryStream_RandomRead_V(void * pThis, 
                                   OPERAND_SIZE offset,
                                   void * pBuffer, 
                                   int iBufferSize, 
                                   OPERAND_SIZE * readBytes,
                                   int flags)
{
    DianaMemoryStream * pStream = (DianaMemoryStream * )pThis;
    OPERAND_SIZE sizeToGive = 0;
    
    if (flags & DIANA_ANALYZE_RANDOM_READ_ABSOLUTE)
    {
        return DI_END_OF_STREAM;
    }
    if (offset >= pStream->bufferSize)
    {
        return DI_END_OF_STREAM;
    }

    sizeToGive = (int)(pStream->bufferSize - offset);
    if (sizeToGive > iBufferSize)
        sizeToGive = iBufferSize;

    DIANA_MEMCPY(pBuffer,(char*)pStream->pBuffer+offset, (DIANA_SIZE_T)sizeToGive);
    *readBytes = sizeToGive;
    return 0;
}

void Diana_InitMemoryStreamEx(DianaMemoryStream * pStream,
                              void * pBuffer,
                              DIANA_SIZE_T bufferSize,
                              int bWritable)
{
    DianaAnalyzeRandomWrite_fnc pWriteFunction = 0;
    if (bWritable)
    {
        pWriteFunction = DianaMemoryStream_RandomWrite_V;
    }
    DianaReadWriteRandomStream_Init(&pStream->parent,
                                    MemoryStream_Read,
                                    DianaMemoryStream_RandomReadMoveTo,
                                    DianaMemoryStream_RandomRead_V,
                                    pWriteFunction);
    pStream->pBuffer = pBuffer;
    pStream->bufferSize = bufferSize;
    pStream->curSize = 0;
}
                              
void Diana_InitMemoryStream(DianaMemoryStream * pStream,
                            void * pBuffer,
                            DIANA_SIZE_T bufferSize)
{
    Diana_InitMemoryStreamEx(pStream, pBuffer, bufferSize, 0);
}


int Diana_ParseCmdOnBuffer(int iMode,
                           void * pBuffer,
                           DIANA_SIZE_T size,
                           DianaBaseGenObject_type * pInitialLine,  // IN
                           DianaParserResult * pResult,  //OUT
                           DIANA_SIZE_T * sizeRead)    // OUT
{
    DianaMemoryStream stream;
    DianaContext context;
    int iRes;

    Diana_InitContext(&context, iMode);
    Diana_InitMemoryStream(&stream, pBuffer, size);
    iRes = Diana_ParseCmd(&context, 
                          pInitialLine, 
                          &stream.parent.parent.parent,  
                          pResult);
    *sizeRead = stream.curSize - context.cacheSize;
    return iRes;
}


int Diana_ParseCmdOnBuffer_testmode(int iMode,
                                    void * pBuffer,
                                    DIANA_SIZE_T size,
                                    DianaBaseGenObject_type * pInitialLine,  // IN
                                    DianaParserResult * pResult,  //OUT
                                    DIANA_SIZE_T * sizeRead)    // OUT
{
    DianaMemoryStream stream;
    DianaContext context;
    int iRes;

    Diana_InitContextWithTestMode(&context, iMode);
    Diana_InitMemoryStream(&stream, pBuffer, size);
    iRes = Diana_ParseCmd(&context, 
                          pInitialLine, 
                          &stream.parent.parent.parent,  
                          pResult);
    *sizeRead = stream.curSize - context.cacheSize;
    return iRes;
}


// common functions
int DianaStreams_CopyData(DianaReadStream * pStream,
                          DianaReadWriteRandomStream * pOutStream,
                          OPERAND_SIZE address,
                          OPERAND_SIZE size,
                          void * pPage,
                          int pageSize)
{
    OPERAND_SIZE addressToWrite = address;
    OPERAND_SIZE sizeToTransfer = size;
    OPERAND_SIZE lastAddressToWrite = address;
    DI_CHECK(Diana_SafeAdd(&lastAddressToWrite, size));
    for(;sizeToTransfer;)
    {
        int readBytes = 0;
        OPERAND_SIZE writeBytes = 0;
        int sizeToUse = pageSize;
        if (sizeToTransfer < (OPERAND_SIZE)sizeToUse)
        {
            sizeToUse = (int)sizeToTransfer;
        }
        DI_CHECK(pStream->pReadFnc(pStream, pPage, sizeToUse, &readBytes));
        if (readBytes != sizeToUse)
        {
            return DI_ERROR;
        }

        DI_CHECK(pOutStream->pRandomWrite(pOutStream, 
                                          addressToWrite,
                                          pPage,
                                          sizeToUse,
                                          &writeBytes,
                                          0));
        
        if (writeBytes != (OPERAND_SIZE)sizeToUse)
        {
            return DI_ERROR;
        }
        addressToWrite += writeBytes;
        sizeToTransfer -= writeBytes;
    }
    return DI_SUCCESS;
}
            
int DianaStreams_MemsetData(DianaReadWriteRandomStream * pOutStream,
                            OPERAND_SIZE address,
                            OPERAND_SIZE size,
                            int value,
                            void * pPage,
                            int pageSize)
{
    OPERAND_SIZE addressToWrite = address;
    OPERAND_SIZE sizeToTransfer = size;
    OPERAND_SIZE lastAddressToWrite = address;
    OPERAND_SIZE sizeToMemsetInPage = (OPERAND_SIZE)pageSize;
    DI_CHECK(Diana_SafeAdd(&lastAddressToWrite, size));

    if (sizeToMemsetInPage > size)
    {
        sizeToMemsetInPage = size;
    }
    DIANA_MEMSET(pPage, value, (DIANA_SIZE_T)sizeToMemsetInPage);
    for(;sizeToTransfer;)
    {
        OPERAND_SIZE writeBytes = 0;
        int sizeToUse = pageSize;
        if (sizeToTransfer < (OPERAND_SIZE)sizeToUse)
        {
            sizeToUse = (int)sizeToTransfer;
        }

        DI_CHECK(pOutStream->pRandomWrite(pOutStream, 
                                          addressToWrite,
                                          pPage,
                                          sizeToUse,
                                          &writeBytes,
                                          0));
        
        if (writeBytes != (OPERAND_SIZE)sizeToUse)
        {
            return DI_ERROR;
        }
        addressToWrite += writeBytes;
        sizeToTransfer -= writeBytes;
    }
    return DI_SUCCESS;
}
