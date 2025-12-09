#pragma once

#include "orthia_process.h"
#include <functional>

namespace orthia
{

class CConsoleProcessImpl:public orthia::CProcess
{
    std::vector<char> m_buffer;
    orthia::CHandleGuard m_readGuard, m_writeGuard;
    orthia::CCriticalSection m_lock;
    std::stringstream m_stream;
    std::wstringstream m_wideStream;
    bool m_createNewConsole = false;
public:
    CConsoleProcessImpl();
    
    void SetCreateNewConsole(bool createNewConsole) {  m_createNewConsole = createNewConsole;   }
    void StartEx(CProcessParams & params,
                  STARTUPINFOW & si,
                  LPSECURITY_ATTRIBUTES lpSec,
                  int bufferSize,
                  orthia::CHandleGuard & readGuard,
                  orthia::CHandleGuard & writeGuard,
                  const std::wstring * pCurrentDirectory);

    void PerformAll();
    bool PerformReadOnce(bool wideChar);
    void PerformAll(std::function<void (const std::string& line)> handler);
    void PerformAll(std::function<void (const std::wstring& line)> handler);

    bool ReadLine(std::vector<char> * pLine);
    bool ReadWideLine(std::vector<wchar_t>* pLine);
};

class CConsoleProcess:public CConsoleProcessImpl, public orthia::RefCountedBase
{
public:
    CConsoleProcess()
    {
    }
};
orthia::intrusive_ptr<CConsoleProcess> StartConsoleProcess(CProcessParams & params,
                                                     const std::wstring * pCurrentDirectory,
                                                     bool bHidden,
                                                     int bufferSize,
                                                     const std::wstring * pDesktop,
                                                     LPSECURITY_ATTRIBUTES lpSec);

//----------
struct IAsyncConsoleProcess:IRefCountedBase
{
    virtual ~IAsyncConsoleProcess(){}
    virtual void Join()=0;
    virtual void Terminate()=0;
};

class CAutoJoiner
{
    CAutoJoiner(const CAutoJoiner&);
    CAutoJoiner & operator = (const CAutoJoiner&);
    orthia::intrusive_ptr<IAsyncConsoleProcess> m_pProcess;
    bool m_terminate;
public:
    CAutoJoiner(orthia::intrusive_ptr<IAsyncConsoleProcess> pProcess,
                bool terminate)
        :   
            m_pProcess(pProcess),
            m_terminate(terminate)
    {
    }
    ~CAutoJoiner()
    {
        if (m_pProcess)
        {
            if (m_terminate)
            {
                m_pProcess->Terminate();
            }
            m_pProcess->Join();
        }
    }
    orthia::intrusive_ptr<IAsyncConsoleProcess> Release()
    {
        orthia::intrusive_ptr<IAsyncConsoleProcess> pProcess = m_pProcess;
        m_pProcess = 0;
        return pProcess;
    }
};
struct IAsyncConsoleProcessObserver:IRefCountedBase
{
    virtual ~IAsyncConsoleProcessObserver(){}
    virtual void OnText(orthia::intrusive_ptr<IAsyncConsoleProcess> pProcess, const std::string & text) =0;
};


orthia::intrusive_ptr<IAsyncConsoleProcess> AsyncStartConsoleProcess(orthia::intrusive_ptr<IAsyncConsoleProcessObserver> pProcessObserver,
                                                             CProcessParams & params,
                                                             const std::wstring * pCurrentDirectory,
                                                             bool bHidden,
                                                             int bufferSize,
                                                             const std::wstring * pDesktop,
                                                             LPSECURITY_ATTRIBUTES lpSec);
}