#include "orthia_processes_ex.h"
#include <thread>

namespace orthia
{

CConsoleProcessImpl::CConsoleProcessImpl()
{
}

void CConsoleProcessImpl::StartEx(CProcessParams & params,
                              STARTUPINFOW & si,
                              LPSECURITY_ATTRIBUTES lpSec,
                              int bufferSize,
                              orthia::CHandleGuard & readGuard,
                              orthia::CHandleGuard & writeGuard,
                              const std::wstring * pCurrentDirectory)
{
    m_buffer.resize(bufferSize);
    std::vector<wchar_t> & vec = params.GetParams();

    m_readGuard.Reset(readGuard.Release());
    m_writeGuard.Reset(writeGuard.Release());

    PROCESS_INFORMATION processInfo;
    memset( &processInfo, 0, sizeof(processInfo) );

    DWORD dwCreationFlags = NORMAL_PRIORITY_CLASS;

    if (m_createNewConsole)
    {
        dwCreationFlags |= CREATE_NEW_CONSOLE;
    }
    // Start the child process. 
    if( !CreateProcessW(NULL,
                        vec.data(),
                        lpSec, 
                        lpSec, 
                        TRUE,
                        dwCreationFlags,
                        NULL, 
                        (pCurrentDirectory ? pCurrentDirectory->c_str(): NULL), 
                        &si, 
                        &processInfo)      
        ) 
    {
        ORTHIA_THROW_WIN32("Can't create process: ["<<orthia::PlatformStringToUtf8(&vec.front())<<"]");
    }

    if (dwCreationFlags & CREATE_SUSPENDED)
    {
        ResumeThread(processInfo.hThread);
    }

    CloseHandle( processInfo.hThread );
    m_process.Reset( processInfo.hProcess );
}
 
void CConsoleProcessImpl::PerformAll()
{
    while(PerformReadOnce(false))
    {
    }
}
void CConsoleProcessImpl::PerformAll(std::function<void(const std::string& line)> handler)
{
    std::vector<char> line;
    std::string strLine;
    while (PerformReadOnce(false))
    {
        for (;ReadLine(&line);)
        {
            if (!line.empty())
            {
                strLine = &line[0];
            }
            handler(strLine);
        }
    }
}
void CConsoleProcessImpl::PerformAll(std::function<void(const std::wstring& line)> handler)
{
    std::vector<wchar_t> line;
    std::wstring strLine;
    while (PerformReadOnce(true))
    {
        for (; ReadWideLine(&line);)
        {
            if (!line.empty())
            {
                strLine = &line[0];
            }
            handler(strLine);
        }
    }
}
bool CConsoleProcessImpl::PerformReadOnce(bool wideChar)
{
    orthia::CAutoCriticalSection guard(m_lock);
    DWORD readBytes = 0;
    m_buffer[0] = 0;
    if (!ReadFile(m_readGuard.Get(), &m_buffer.front(), (DWORD)m_buffer.size()-2, &readBytes, 0))
    {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_BROKEN_PIPE)
            return false;

        std::wstring data = L"The process failed";
        ORTHIA_THROW_WIN32(orthia::PlatformStringToUtf8(data));
    }
    if (wideChar)
    {
        m_wideStream = std::wstringstream();
        m_wideStream.write((wchar_t*)m_buffer.data(), readBytes/2);
    }
    else
    {
        m_stream = std::stringstream();
        m_stream.write(&m_buffer.front(), readBytes);
    }
    return true;
}

bool CConsoleProcessImpl::ReadLine(std::vector<char> * pLine)
{
    orthia::CAutoCriticalSection guard(m_lock);
    if (m_stream.eof())
    {
        return false;
    }
    if (pLine->empty())
    {
        pLine->resize(m_buffer.size() < 1024? 1024: m_buffer.size());
    }
    (*pLine)[0] = 0;
    m_stream.getline(&pLine->front(), pLine->size());
    if(m_stream.fail())
    {
        return false;
    }
    return true;
}
bool CConsoleProcessImpl::ReadWideLine(std::vector<wchar_t>* pLine)
{
    orthia::CAutoCriticalSection guard(m_lock);
    if (m_wideStream.eof())
    {
        return false;
    }
    if (pLine->empty())
    {
        pLine->resize(m_buffer.size() < 1024 ? 1024 : m_buffer.size());
    }
    (*pLine)[0] = 0;
    m_wideStream.getline(&pLine->front(), pLine->size());
    if (m_wideStream.fail())
    {
        return false;
    }
    return true;
}
template<class ControllerType>
orthia::intrusive_ptr<ControllerType> StartConsoleProcessImpl(CProcessParams & params,
                                                         const std::wstring * pCurrentDirectory,
                                                         bool bHidden,
                                                         int bufferSize,
                                                         const std::wstring * pDesktop,
                                                         LPSECURITY_ATTRIBUTES lpSec)
{
    SECURITY_ATTRIBUTES saAttr; 
    memset(&saAttr, 0, sizeof(saAttr));
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    HANDLE hReadPipe=0, hWritePipe = 0;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 1024*1024))
    {
        ORTHIA_THROW_WIN32("Can't create pipe");
    }

    orthia::CHandleGuard readGuard(hReadPipe), writeGuard(hWritePipe);

    HANDLE hReadPipe2=0, hWritePipe2 = 0;
    if (!CreatePipe(&hReadPipe2, &hWritePipe2, &saAttr, 1024*1024))
    {
        ORTHIA_THROW_WIN32("Can't create pipe");
    }
    orthia::CHandleGuard readGuard2(hReadPipe2), writeGuard2(hWritePipe2);

    STARTUPINFOW si;
    memset( &si, 0, sizeof(si) );
    si.cb = sizeof(si);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.hStdInput = hReadPipe2;
    si.dwFlags |= STARTF_USESTDHANDLES;
    if (bHidden)
    {
        si.wShowWindow = SW_HIDE;
        si.dwFlags |= STARTF_USESHOWWINDOW;
    }
    if (pDesktop)
    {
        si.lpDesktop = (LPWSTR)pDesktop->c_str();
    }
    
    if (!SetHandleInformation(hWritePipe2, HANDLE_FLAG_INHERIT, 0))
    {
        ORTHIA_THROW_WIN32("Can't set handle information");
    }
    if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0))
    {
        ORTHIA_THROW_WIN32("Can't set handle information");
    }
    
    orthia::intrusive_ptr<ControllerType> pProcess(new ControllerType());
    if (params.NeedToCreateNewConsole())
    {
        pProcess->SetCreateNewConsole(true);
    }
    pProcess->StartEx(params,
                      si,
                      lpSec,
                      bufferSize,
                      readGuard,
                      writeGuard2,
                      pCurrentDirectory);

    readGuard2.Reset(0);
    writeGuard.Reset(0);
    return pProcess;

}


orthia::intrusive_ptr<orthia::CConsoleProcess> StartConsoleProcess(CProcessParams & params,
                                                         const std::wstring * pCurrentDirectory,
                                                         bool bHidden,
                                                         int bufferSize,
                                                         const std::wstring * pDesktop,
                                                         LPSECURITY_ATTRIBUTES lpSec)
{
    return StartConsoleProcessImpl<orthia::CConsoleProcess>(params,
                                                          pCurrentDirectory,
                                                          bHidden,
                                                          bufferSize,
                                                          pDesktop,
                                                          lpSec);
}

//-----------
class CAsyncConsoleProcess:public orthia::RefCountedBase_t<IAsyncConsoleProcess>, 
                           public CConsoleProcessImpl
{
    orthia::intrusive_ptr<IAsyncConsoleProcessObserver> m_pProcess;
    std::thread m_thread;
public:
    CAsyncConsoleProcess()
    {
    }
    void StartThread(orthia::intrusive_ptr<IAsyncConsoleProcessObserver> pProcess)
    {
        m_pProcess = pProcess;
        orthia::intrusive_ptr<CAsyncConsoleProcess> sharedThis = this;
        m_thread = std::thread([sharedThis = sharedThis] () mutable { sharedThis->Execute(); } );
    }
    virtual void Join()
    {
        orthia::CProcess::Join();
    }
    virtual void Terminate()
    {
        orthia::CProcess::AsyncTerminate();
    }
    unsigned int Execute()
    {
        try
        {
            std::vector<char> line;
            std::string strLine;
            while(PerformReadOnce(false))
            {
                for (; ReadLine(&line);)
                {
                    if (!line.empty())
                    {
                        strLine = &line[0];
                    }
                    m_pProcess->OnText(this, strLine);
                }
            }
        }
        catch(std::exception & )
        {
        }
        return 0;
    }
};

orthia::intrusive_ptr<IAsyncConsoleProcess> AsyncStartConsoleProcess(orthia::intrusive_ptr<IAsyncConsoleProcessObserver> pProcessObserver,
                                                             CProcessParams & params,
                                                             const std::wstring * pCurrentDirectory,
                                                             bool bHidden,
                                                             int bufferSize,
                                                             const std::wstring * pDesktop,
                                                             LPSECURITY_ATTRIBUTES lpSec)
{
    orthia::intrusive_ptr<CAsyncConsoleProcess> pResult =
        StartConsoleProcessImpl<orthia::CAsyncConsoleProcess>(params,
                                                          pCurrentDirectory,
                                                          bHidden,
                                                          bufferSize,
                                                          pDesktop,
                                                          lpSec);
    pResult->StartThread(pProcessObserver);
    return pResult;
}

}