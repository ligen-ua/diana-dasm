#include "diana_debugger.h"

#include "vector"

namespace diana
{

    int g_step = 10000;
void PrintContext(const CONTEXT & context)
{
    std::hex(std::cout);
    std::cout<<"[";
    std::cout<<context.SegCs<<"-";
    std::cout<<context.Eip<<"-";
    std::cout<<context.Edi<<"-";
    std::cout<<context.Esi<<"-";
    std::cout<<context.Ebx<<"-";
    std::cout<<context.Edx<<"-";
    std::cout<<context.Ecx<<"-";
    std::cout<<context.Eax<<"-";
    std::cout<<context.Ebp<<"-";
    std::cout<<context.EFlags<<"-";
    std::cout<<context.Esp<<"-";
    std::cout<<context.SegSs;
    std::cout<<"]\n\n";
    std::cout.flush();
}

CDianaDebugger::CDianaDebugger(const std::wstring & exeName)
    :
        m_exeName(exeName),
        m_cmdCount(0),
        m_tempCount(0),
        m_bValidPrevInfo(false)
{
    int code = DianaWin32Processor_InitEx(&m_processor, &m_remoteStream, 0, 0);
    if (code != DI_SUCCESS)
    {
        throw DianaException(code);
    }
}

void CompareContexts(CONTEXT & context1, CONTEXT & context2)
{
#define COMPARE_CHECK(X) \
    if (context1.##X != context2.##X)  __asm int 3;   __asm nop;
//

    COMPARE_CHECK(SegGs);
    COMPARE_CHECK(SegFs);
    COMPARE_CHECK(SegEs);
    COMPARE_CHECK(SegDs);
   
    COMPARE_CHECK(Edi);
    COMPARE_CHECK(Esi);
    COMPARE_CHECK(Ebx);
    COMPARE_CHECK(Edx);
    COMPARE_CHECK(Ecx);
    COMPARE_CHECK(Eax);

    COMPARE_CHECK(Ebp);
    COMPARE_CHECK(Eip);
    COMPARE_CHECK(SegCs);              // MUST BE SANITIZED
//    COMPARE_CHECK(EFlags);             // MUST BE SANITIZED
    COMPARE_CHECK(Esp);
    COMPARE_CHECK(SegSs);
}

void CDianaDebugger::DebugLoop()
{
    DEBUG_EVENT debugEvent;
    while(1)
    {
        BOOL waitSuccess = WaitForDebugEvent(&debugEvent, 
                                             INFINITE);

        if (!waitSuccess)
            throw Win32Exception(::GetLastError());

        if(debugEvent.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
        {
            break;
        }

        if(debugEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
        {
            if (debugEvent.u.Exception.ExceptionRecord.ExceptionCode == STATUS_BREAKPOINT ||
                debugEvent.u.Exception.ExceptionRecord.ExceptionCode == STATUS_SINGLE_STEP)
            {
                memset(&m_context, 0, sizeof(m_context));
                m_context.ContextFlags = CONTEXT_FULL;
                if (!GetThreadContext(m_hThread.get(), &m_context))
                {
                    DWORD status = GetLastError();
                    throw Win32Exception("Can't get thread context", status);
                }

                // compare data
                if (m_bValidPrevInfo)
                {
                    CompareContexts(m_context, m_dianaContext);
                    m_remoteStream.Validate();
                }

                ++m_tempCount;
                ++m_cmdCount;
                
                if (m_tempCount >= g_step)
                {
                    std::cout<<"Commands: "<<m_cmdCount<<"\n";
                    m_tempCount = 0;
                }
                m_context.EFlags |= 0x000100;

                if (!SetThreadContext(m_hThread.get(), &m_context))
                {
                    DWORD status = GetLastError();
                    throw Win32Exception("Can't set thread context", status);
                }

                // run next command by diana
                m_remoteStream.Clear();

                DianaProcessor_LoadContext32(&m_processor.m_processor, &m_context);
                DianaProcessor_SaveContext32(&m_processor.m_processor, &m_prevDiContext);
                int res = DianaProcessor_ExecOnce(&m_processor.m_processor);
                switch (res)
                {
                default:
                    __asm int 3;
                case DI_ERROR_NOT_USED_BY_CORE:
                    m_bValidPrevInfo = false;
                    break;
                case DI_SUCCESS:
                    m_dianaContext = m_context;
                    DianaProcessor_SaveContext32(&m_processor.m_processor, &m_dianaContext);
                    if (m_dianaContext.Eip == m_prevDiContext.Eip)
                    {
                        __asm int 3;
                    }
                    m_bValidPrevInfo = true;
                    break;
                }
            }
        }

        ContinueDebugEvent(debugEvent.dwProcessId, 
                           debugEvent.dwThreadId, 
                           DBG_CONTINUE  );
    }
}

void CDianaDebugger::StartProcess()
{
    STARTUPINFOW si;
    memset( &si, 0, sizeof(si) );
    si.cb = sizeof(si);


    PROCESS_INFORMATION processInfo;
    memset( &processInfo, 0, sizeof(processInfo) );

    std::vector<wchar_t> cmdLine;
    cmdLine.reserve(m_exeName.size()+1);   
    cmdLine.assign(m_exeName.begin(), m_exeName.end());
    cmdLine.push_back(0);

    // Start the child process. 
    if( !CreateProcessW(m_exeName.empty()?NULL:m_exeName.c_str(), 
                        &cmdLine.front(), 
                        0, 
                        0, 
                        TRUE,        
                        DEBUG_PROCESS|DEBUG_ONLY_THIS_PROCESS|NORMAL_PRIORITY_CLASS|CREATE_SUSPENDED,
                        NULL, 
                        NULL, 
                        &si, 
                        &processInfo)      
        ) 
    {
        DWORD status = GetLastError();
        throw Win32Exception("Can't create process", status);
    }
    m_hThread.reset( processInfo.hThread );
    m_hProcess.reset( processInfo.hProcess );

    CONTEXT context;
    memset(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(processInfo.hThread, &context))
    {
        DWORD status = GetLastError();
        throw Win32Exception("Can't get thread context", status);
    }

    context.EFlags |= 0x000100;

    if (!SetThreadContext(processInfo.hThread, &context))
    {
        DWORD status = GetLastError();
        throw Win32Exception("Can't set thread context", status);
    }

}

void CDianaDebugger::Start()
{
    StartProcess();

    m_remoteStream.Init(m_hProcess.get());

    ResumeThread(m_hThread.get());
    DebugLoop();
}

}