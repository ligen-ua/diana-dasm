#include "test_common.h"
extern "C"
{
#include "diana_streams.h"
}
int g_diana_resultCode = 0;

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

    if (pResult->iFullCmdSize!=size || *sizeRead!=size)
    {
        std::cout<<"[ERROR] instruction size "<<pResult->iFullCmdSize<<", read "<<*sizeRead<<", buffer "<<size<<"\n";
        Diana_FatalBreak();
        return DIANA_TEST_ERROR_SIZE;
    }
    return iRes;
}

int Diana_ParseCmdOnBufferEx_test(int iMode,
                                  int iArch,
                                  void * pBuffer,
                                  size_t size,
                                  DianaBaseGenObject_type * pInitialLine, // IN
                                  DianaParserResult * pResult,            // OUT
                                  size_t * sizeRead)                      // OUT
{
    int iRes = Diana_ParseCmdOnBufferEx(iMode,
                                        iArch,
                                        pBuffer,
                                        size,
                                        pInitialLine, // IN
                                        pResult,      // OUT
                                        sizeRead);    // OUT
    if (iRes)
        return iRes;

    if (pResult->iFullCmdSize!=size || *sizeRead!=size)
    {
        std::cout<<"[ERROR] instruction size "<<pResult->iFullCmdSize<<", read "<<*sizeRead<<", buffer "<<size<<"\n";
        Diana_FatalBreak();
        return DIANA_TEST_ERROR_SIZE;
    }
    return iRes;
}
