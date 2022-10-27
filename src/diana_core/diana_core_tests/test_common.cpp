#include "test_common.h"
extern "C"
{
#include "diana_streams.h"
}

int Diana_ParseCmdOnBuffer_test(int iMode,
                                void * pBuffer,
                                size_t size,
                                DianaBaseGenObject_type * pInitialLine, // IN
                                DianaParserResult * pResult,            // OUT
                                size_t * sizeRead)                      // OUT
{
    int iRes = Diana_ParseCmdOnBuffer(iMode,
                                      pBuffer,
                                      size,
                                      pInitialLine, // IN
                                      pResult,      // OUT
                                      sizeRead);    // OUT
    if (iRes)
        return iRes;

    if (pResult->iFullCmdSize!=size)
        Diana_FatalBreak();

    if (*sizeRead!=size)
        Diana_FatalBreak();
    return iRes;
}
