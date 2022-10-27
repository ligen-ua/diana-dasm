#include "diana_stream_proxy.h"

static int StreamProxy_Read(void * pThis, void * pBuffer, int iBufferSize, int * readBytes)
{
    DianaStreamProxy * pStream = (DianaStreamProxy * )pThis;
    if (pStream->tail_size)
    {
        if (pStream->tail_size >= iBufferSize)
        {
            DIANA_MEMCPY(pBuffer, pStream->pBuffer + pStream->begin, iBufferSize);
            pStream->tail_size -= iBufferSize;
            pStream->begin+=iBufferSize;
            *readBytes = iBufferSize;
            return 0;
        }
        else
        {
            int iRes = 0;
            DIANA_MEMCPY(pBuffer, pStream->pBuffer + pStream->begin, pStream->tail_size);

            // read from stream
            iRes = 
            pStream->pUsedStream->pReadFnc(pStream->pUsedStream, 
                                           (char*)pBuffer + pStream->tail_size,
                                           iBufferSize - pStream->tail_size,
                                           readBytes);
            *readBytes += pStream->tail_size;
            pStream->tail_size = 0;
            return iRes;
        }
    }
    // direct mode
    return pStream->pUsedStream->pReadFnc(pStream->pUsedStream, 
                                          pBuffer,
                                          iBufferSize,
                                          readBytes);

}

void Diana_InitStreamProxy(DianaStreamProxy * pStream, 
                           DianaReadStream * pUsedStream,
                           unsigned char * pBuffer, 
                           int readBytes)
{
    pStream->pUsedStream = pUsedStream;
    pStream->tail_size = readBytes; 
    pStream->begin = 0;
    pStream->parent.pReadFnc = StreamProxy_Read;
    pStream->pBuffer = pBuffer;
}
