#pragma once

#include "orthia_utils.h"
namespace orthia
{


class CProcessParams
{
private:
    std::vector<wchar_t> m_params;
    size_t m_originalSize;
    bool m_createNewConsole;
public:
    CProcessParams(const std::wstring & exe, bool createNewConsole = false);
    CProcessParams & operator << (const std::wstring & param);
    CProcessParams& operator << (int param);

    std::vector<wchar_t> & GetParams(); 
    bool NeedToCreateNewConsole() const { return m_createNewConsole; }
};

class CProcess
{
protected:
    orthia::CHandleGuard m_process;
    orthia::CHandleGuard m_job;
public:
    CProcess();
    virtual ~CProcess();

    void Start(CProcessParams & params);
    void Start(CProcessParams & params, bool bHide);

    void StartEx(CProcessParams & params, 
                 STARTUPINFO * pStartupInfo);
    void SyncTerminate();
    void AsyncTerminate(UINT exitCode);
    void AsyncTerminate();
    void Join();
    HANDLE GetHandle();
    DWORD GetExitCode();
};

class CProcessStopper
{
    CProcess * m_pProcess;
public:
    CProcessStopper(CProcess * pProcess)
        : m_pProcess(pProcess)
    {
    }
    void release()
    {
        m_pProcess = 0;
    }
    ~CProcessStopper()
    {
        if (m_pProcess)
            m_pProcess->SyncTerminate();
    }
};

} // namespace

