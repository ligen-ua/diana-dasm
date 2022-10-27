#ifndef DIANA_STREAM_PROXY
#define DIANA_STREAM_PROXY

#include "diana_core.h"

typedef struct _dianaStreamProxy
{
    DianaReadStream parent;
    DianaReadStream * pUsedStream;
    unsigned char * pBuffer;
    int begin;
    int tail_size;

}DianaStreamProxy;

void Diana_InitStreamProxy(DianaStreamProxy * pStream,  // this
                           DianaReadStream * pUsedStream,
                           unsigned char * pBuffer, 
                           int readBytes);
#endif