#include "diana_debug_stream.h"


namespace diana
{

int CDebugRemoteStream::DianaWin32RandomWrite_Remote(void * pThis, 
                                        OPERAND_SIZE selector,
                                        OPERAND_SIZE offset,
                                        void * pBuffer, 
                                        OPERAND_SIZE iBufferSize, 
                                        OPERAND_SIZE * wrote,
                                        struct _dianaProcessor * pProcessor,
                                        DianaUnifiedRegister segReg)
{
    CDebugRemoteStream * p = (CDebugRemoteStream *)pThis;

    if (segReg != reg_DS && segReg != reg_CS && segReg != reg_SS)
        return DI_ERROR_NOT_USED_BY_CORE;


    std::pair<WriteMap_type::iterator, bool> res = p->m_writeMap.insert(std::make_pair(offset, DataVector_type()));
    if (!res.second)
    {
        _asm int 3;
        return DI_ERROR_NOT_USED_BY_CORE;
    }
    res.first->second.assign((char*)pBuffer, 
                             (char*)pBuffer + iBufferSize);
    *wrote = iBufferSize;
    return DI_SUCCESS;
}

int CDebugRemoteStream::DianaWin32RandomRead_Remote(void * pThis, 
                                       OPERAND_SIZE selector,
                                       OPERAND_SIZE offset,
                                       void * pBuffer, 
                                       OPERAND_SIZE iBufferSize, 
                                       OPERAND_SIZE * readed,
                                       struct _dianaProcessor * pProcessor,
                                       DianaUnifiedRegister segReg)
{
    CDebugRemoteStream * p = (CDebugRemoteStream *)pThis;
    
    if (segReg != reg_DS && segReg != reg_CS && segReg != reg_SS)
        return DI_ERROR_NOT_USED_BY_CORE;

    {
        SIZE_T res = 0;
        if (!ReadProcessMemory(p->m_hProcess, 
                            (void*)offset, 
                            pBuffer, 
                            (SIZE_T)iBufferSize,
                            &res))
        {
            return DI_WIN32_ERROR;
        }
        *readed = res;
    }
    return DI_SUCCESS;
}

CDebugRemoteStream::CDebugRemoteStream(HANDLE hProcess)
    : 
        m_hProcess(hProcess)
{
    pReadFnc = DianaWin32RandomRead_Remote;
    pWriteFnc = DianaWin32RandomWrite_Remote;
}

void CDebugRemoteStream::Init(HANDLE hProcess)
{
    m_hProcess = hProcess;
}
void CDebugRemoteStream::Clear()
{
    m_writeMap.clear();
}
void CDebugRemoteStream::Validate()
{
    std::vector<char> buf;
    for(WriteMap_type::iterator it = m_writeMap.begin(), it_end = m_writeMap.end();
        it != it_end;
        ++it)
    {
        buf.resize(it->second.size());
        
        SIZE_T res = 0;
        if (!ReadProcessMemory(m_hProcess, 
                               (LPCVOID)it->first, 
                               &buf.front(), 
                               buf.size(),
                               &res))
        {
            __asm int 3;
        }

        if (buf != it->second)
        {
            __asm int 3;
        }
    }
}

}// diana