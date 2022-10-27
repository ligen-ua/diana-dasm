#include "orthia_streams.h"

namespace orthia
{

bool IsDefaultSelector(DianaUnifiedRegister selector)
{
    switch(selector)
    {
    case reg_none:
    case reg_CS:
    case reg_DS:
    case reg_SS:
    case reg_ES:
        return true;
    }
    return false;
}
DianaMemoryStream::DianaMemoryStream(Address_type currentOffset, 
                                     IMemoryReader * pMemoryReader,
                                     OPERAND_SIZE moduleSize)
         : 
            m_pMemoryReader(pMemoryReader),
            m_currentOffset(currentOffset),
            m_moduleSize(moduleSize)
{
    DianaMovableReadStream_Init(&parent,
                                DianaRead,
                                DianaAnalyzeMoveTo,
                                DianaRandomRead);
    pRandomWrite = 0;
    m_pMemoryReaderWriter = 0;
}

DianaMemoryStream::DianaMemoryStream(Address_type currentOffset, 
                                     IMemoryReaderWriter * pMemoryReaderWriter,
                                     OPERAND_SIZE moduleSize)
         : 
            m_pMemoryReader(pMemoryReaderWriter),
            m_currentOffset(currentOffset),
            m_moduleSize(moduleSize)
{
    DianaMovableReadStream_Init(&parent,
                                DianaRead,
                                DianaAnalyzeMoveTo,
                                DianaRandomRead);
    m_pMemoryReaderWriter = pMemoryReaderWriter;
    pRandomWrite = DianaRandomWrite;
}

int DianaAnalyzeMoveTo(void * pThis, 
                       OPERAND_SIZE offset)
{
    DianaMemoryStream * pStream = (DianaMemoryStream * )pThis;

    if (pStream->m_moduleSize)
    {
        if (offset > pStream->m_moduleSize)
            return DI_END_OF_STREAM;
    }
    pStream->m_currentOffset = offset;
    return DI_SUCCESS;
}

int DianaRead(void * pThis, 
              void * pBuffer, 
              int iBufferSize, 
              int * bytesRead)
{
    try
    {
        Address_type bytesRead2 = 0;
        DianaMemoryStream * pStream = (DianaMemoryStream * )pThis;
        pStream->m_pMemoryReader->Read(pStream->m_currentOffset, 
                                    iBufferSize,
                                    pBuffer,
                                    &bytesRead2,
                                    0,
                                    0,
                                    reg_none);
        *bytesRead = (int)bytesRead2;
        pStream->m_currentOffset += (Address_type)bytesRead2;
        if (!bytesRead2)
        {
            return DI_ERROR;
        }
        return DI_SUCCESS;
    }
    catch(std::exception & ex)
    {
        &ex;
        return DI_ERROR;
    }
}
 
int DianaRandomRead(void * pThis, 
                       OPERAND_SIZE offset,
                       void * pBuffer, 
                       int iBufferSize, 
                       OPERAND_SIZE * readBytes,
                       int flags)
{
    try
    {
        int orthiaFlags = 0;
        if (flags & DIANA_ANALYZE_RANDOM_READ_ABSOLUTE)
        {
            orthiaFlags |= ORTHIA_MR_FLAG_READ_ABSOLUTE;
        }

        Address_type bytesRead2 = 0;
        DianaMemoryStream * pStream = (DianaMemoryStream * )pThis;
        pStream->m_pMemoryReader->Read(offset, 
                                    iBufferSize,
                                    pBuffer,
                                    &bytesRead2,
                                    orthiaFlags,
                                    0,
                                    reg_none);
        *readBytes = bytesRead2;
        return DI_SUCCESS;
    }
    catch(std::exception & ex)
    {
        &ex;
        return DI_ERROR;
    }
}

int DianaRandomWrite(void * pThis, 
                       OPERAND_SIZE offset,
                       void * pBuffer, 
                       int iBufferSize, 
                       OPERAND_SIZE * readBytes,
                       int flags)
{
    try
    {
        int orthiaFlags = 0;
        if (flags & DIANA_ANALYZE_RANDOM_READ_ABSOLUTE)
        {
            orthiaFlags |= ORTHIA_MR_FLAG_READ_ABSOLUTE;
        }

        Address_type bytesRead2 = 0;
        DianaMemoryStream * pStream = (DianaMemoryStream * )pThis;
        pStream->m_pMemoryReaderWriter->Write(offset, 
                                              iBufferSize,
                                              pBuffer,
                                              &bytesRead2,
                                              orthiaFlags,
                                              0,
                                              reg_none);
        *readBytes = bytesRead2;
        return DI_SUCCESS;
    }
    catch(std::exception & ex)
    {
        &ex;
        return DI_ERROR;
    }
}


static int ProcessorDianaRandomRead(void * pThis, 
                                    OPERAND_SIZE selector,
                                    OPERAND_SIZE offset,
                                    void * pBuffer, 
                                    OPERAND_SIZE iBufferSize, 
                                    OPERAND_SIZE * readed,
                                    struct _dianaProcessor * pProcessor,
                                    DianaUnifiedRegister segReg)
{
    try
    {
        Address_type selectorArg = 0;
        if (!IsDefaultSelector(segReg))
        {
            selectorArg = selector;
        }
        ProcessorReadWriteStream * pStream = (ProcessorReadWriteStream * )pThis;
        pStream->m_pMemoryReaderWriter->Read(offset, 
                                              iBufferSize,
                                              pBuffer,
                                              readed,
                                              0,
                                              selectorArg,
                                              segReg);
        return DI_SUCCESS;
    }
    catch(std::exception & ex)
    {
        &ex;
        return DI_ERROR;
    }
}
static int ProcessorDianaRandomWrite(void * pThis, 
                                     OPERAND_SIZE selector,
                                     OPERAND_SIZE offset,
                                     void * pBuffer, 
                                     OPERAND_SIZE iBufferSize, 
                                     OPERAND_SIZE * wrote,
                                     struct _dianaProcessor * pProcessor,
                                     DianaUnifiedRegister segReg)
{
    try
    {
        Address_type selectorArg = 0;
        if (!IsDefaultSelector(segReg))
        {
            selectorArg = selector;
        }
        ProcessorReadWriteStream * pStream = (ProcessorReadWriteStream * )pThis;
        pStream->m_pMemoryReaderWriter->Write(offset, 
                                              iBufferSize,
                                              pBuffer,
                                              wrote,
                                              0,
                                              selectorArg,
                                              segReg);
        return DI_SUCCESS;
    }
    catch(std::exception & ex)
    {
        &ex;
        return DI_ERROR;
    }
}
ProcessorReadWriteStream::ProcessorReadWriteStream(IMemoryReaderWriter * pMemoryReaderWriter)
    :
        m_pMemoryReaderWriter(pMemoryReaderWriter)
{
    pReadFnc = ProcessorDianaRandomRead;
    pWriteFnc = ProcessorDianaRandomWrite;

}
// DianaAnalyzerReadWriteStream
static int DianaAnalyzerReadWriteStream_DianaAnalyzeRandomRead(void * pThis, 
                                                           OPERAND_SIZE offset,
                                                           void * pBuffer, 
                                                           int iBufferSize, 
                                                           OPERAND_SIZE * readBytes,
                                                           int flags)
{
    try
    {
        DianaAnalyzerReadWriteStream * pStream = (DianaAnalyzerReadWriteStream * )pThis;
        pStream->m_pMemoryReaderWriter->Read(offset, 
                                              iBufferSize,
                                              pBuffer,
                                              readBytes,
                                              0,
                                              0,
                                              reg_none);
        return DI_SUCCESS;
    }
    catch(std::exception & ex)
    {
        &ex;
        return DI_ERROR;
    }
}

static int DianaAnalyzerReadWriteStream_DianaAnalyzeRandomWrite(void * pThis, 
                                                             OPERAND_SIZE offset,
                                                             void * pBuffer, 
                                                             int iBufferSize, 
                                                             OPERAND_SIZE * writeBytes,
                                                             int flags)
{
    try
    {
        DianaAnalyzerReadWriteStream * pStream = (DianaAnalyzerReadWriteStream * )pThis;
        pStream->m_pMemoryReaderWriter->Write(offset, 
                                              iBufferSize,
                                              pBuffer,
                                              writeBytes,
                                              ORTHIA_MR_FLAG_WRITE_ALLOCATE,
                                              0,
                                              reg_none);
        return DI_SUCCESS;
    }
    catch(std::exception & ex)
    {
        &ex;
        return DI_ERROR;
    }
}

static int DianaAnalyzerReadWriteStream_DianaAnalyzeMoveTo(void * pThis, 
                                                           OPERAND_SIZE offset)
{
    DianaAnalyzerReadWriteStream * pStream = (DianaAnalyzerReadWriteStream * )pThis;
    pStream->m_currentPosition = offset;
    return DI_SUCCESS;
}
static int DianaAnalyzerReadWriteStream_DianaRead(void * pThis,
                                              void * pBuffer,
                                              int iBufferSize,
                                              int * readBytes)
{
    DianaAnalyzerReadWriteStream * pStream = (DianaAnalyzerReadWriteStream * )pThis;

    OPERAND_SIZE readBytes64 = 0;
    DI_CHECK(DianaAnalyzerReadWriteStream_DianaAnalyzeRandomRead(pThis, 
                                                                 pStream->m_currentPosition,
                                                                 pBuffer,
                                                                 iBufferSize,
                                                                 &readBytes64,
                                                                 0));
    DI_CHECK(Diana_SafeAdd(&pStream->m_currentPosition, readBytes64));
    *readBytes = (int)readBytes64;
    return DI_SUCCESS;
}
DianaAnalyzerReadWriteStream::DianaAnalyzerReadWriteStream(IMemoryReaderWriter * pMemoryReaderWriter)
    :
        m_pMemoryReaderWriter(pMemoryReaderWriter),
        m_currentPosition(0)
{
     DianaReadWriteRandomStream_Init(this,
                                     DianaAnalyzerReadWriteStream_DianaRead, 
                                     DianaAnalyzerReadWriteStream_DianaAnalyzeMoveTo,
                                     DianaAnalyzerReadWriteStream_DianaAnalyzeRandomRead,
                                     DianaAnalyzerReadWriteStream_DianaAnalyzeRandomWrite);
}

}