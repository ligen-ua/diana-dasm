#include "orthia_process.h"

namespace orthia
{

CProcessParams::CProcessParams(const std::wstring & exe, bool createNewConsole)
    :
        m_originalSize(0), m_createNewConsole(createNewConsole)
{
    m_params.reserve(exe.size() + 1024);
    m_params.push_back(L'\"');
    m_params.insert(m_params.end(), exe.begin(), exe.end());
    m_params.push_back(L'\"');
}
CProcessParams & CProcessParams::operator << (const std::wstring & param)
{
    if (param.find('\"') != param.npos) 
    {
        throw std::runtime_error("Invalid params");
    }
    while (m_params.size() + param.size() + 4 > m_params.capacity())
        m_params.reserve(m_params.capacity()*2);

    m_params.push_back(L' ');
    m_params.push_back(L'\"');
    m_params.insert(m_params.end(), param.begin(), param.end());
    m_params.push_back(L'\"');
    return *this;
}
CProcessParams& CProcessParams::operator << (int param)
{
    m_params.push_back(L' ');
    std::wstring sParam = std::to_wstring(param);
    m_params.insert(m_params.end(), sParam.begin(), sParam.end());
    return *this;
}
std::vector<wchar_t> & CProcessParams::GetParams()
{
    if (!m_originalSize)
    {
        size_t originalSize = m_params.size();
        m_params.resize(originalSize + 1);
        m_originalSize = originalSize;
    }
    return m_params;
} 

CProcess::CProcess()
{
}
CProcess::~CProcess()
{
}

void CProcess::StartEx(CProcessParams& params,
    STARTUPINFO* pStartupInfo)
{
    std::vector<wchar_t>& vec = params.GetParams();

    PROCESS_INFORMATION processInfo;

    memset(&processInfo, 0, sizeof(processInfo));

    BOOL bRes = FALSE;
    DWORD dwCreationFlags = NORMAL_PRIORITY_CLASS;

    bRes = CreateProcess(NULL,
        &vec.front(),
        NULL,
        NULL,
        FALSE,
        dwCreationFlags,
        NULL,
        NULL,
        pStartupInfo,
        &processInfo);

    if(!bRes)
    {
        ORTHIA_THROW_WIN32("Can't start process: "+orthia::Utf16ToUtf8(&vec.front()));
    }

    if (dwCreationFlags & CREATE_SUSPENDED)
    {
        ResumeThread(processInfo.hThread);
    }
    m_process.Reset(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
}
void CProcess::Start(CProcessParams & params, bool bHide)
{
    STARTUPINFO startupInfo;
    memset(&startupInfo, 0, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    if (bHide)
    {
        startupInfo.dwFlags = STARTF_USESHOWWINDOW;
        startupInfo.wShowWindow = SW_HIDE;
    }

    StartEx(params, &startupInfo);
}
void CProcess::Start(CProcessParams & params)
{
    STARTUPINFO startupInfo;
    memset(&startupInfo, 0, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    StartEx(params, &startupInfo);
}

void CProcess::Join()
{
    if (m_process.Get())
    {
        WaitForSingleObject( m_process.Get(), INFINITE);
    }
}

DWORD CProcess::GetExitCode()
{
    DWORD dwExitCode = 0;
    if (!GetExitCodeProcess(GetHandle(), &dwExitCode))
    {
        return GetLastError();
    }
    return dwExitCode;
}
HANDLE CProcess::GetHandle()
{
    return m_process.Get();
}
void CProcess::SyncTerminate()
{
    AsyncTerminate();
    if (m_process.Get())
    {
        WaitForSingleObject( m_process.Get(), INFINITE);
    }
}
void CProcess::AsyncTerminate()
{
    AsyncTerminate(2);
}
void CProcess::AsyncTerminate(UINT exitCode)
{
    if (m_process.Get())
    {
        TerminateProcess(m_process.Get(), exitCode);
    }
}


} // namespace
