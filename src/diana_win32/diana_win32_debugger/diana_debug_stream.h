#ifndef DIANA_DEBUG_STREAM_H
#define DIANA_DEBUG_STREAM_H

extern "C"
{
#include "diana_win32_streams.h"
}
#include "map"
#include "vector"

struct _dianaProcessor;
namespace diana
{

class CDebugRemoteStream:public DianaRandomReadWriteStream
{
    typedef std::vector<char> DataVector_type;
    typedef std::map<OPERAND_SIZE, DataVector_type> WriteMap_type;

    WriteMap_type m_writeMap;

    HANDLE m_hProcess;

    static int DianaWin32RandomWrite_Remote(void * pThis, 
                                        OPERAND_SIZE selector,
                                        OPERAND_SIZE offset,
                                        void * pBuffer, 
                                        OPERAND_SIZE iBufferSize, 
                                        OPERAND_SIZE * wrote,
                                         struct _dianaProcessor * pProcessor,
                                        DianaUnifiedRegister segReg);

    static int DianaWin32RandomRead_Remote(void * pThis, 
                                        OPERAND_SIZE selector,
                                        OPERAND_SIZE offset,
                                        void * pBuffer, 
                                        OPERAND_SIZE iBufferSize, 
                                        OPERAND_SIZE * readed,
                                        struct _dianaProcessor * pProcessor,
                                        DianaUnifiedRegister segReg);
public:
    CDebugRemoteStream(HANDLE hProcess=0);

    void Init(HANDLE hProcess);
    void Validate();
    void Clear();
};

}// diana
#endif