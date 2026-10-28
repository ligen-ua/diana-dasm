#include "oui_processes.h"
#include <cstdlib>
#include <sstream>

namespace oui
{
    bool ParseThreadSyscallLine(const std::string& line, ThreadSyscallState& state)
    {
        state = ThreadSyscallState();
        std::istringstream stream(line);
        std::vector<std::string> fields;
        for (std::string field; stream >> field;)
        {
            fields.push_back(field);
        }
        if (fields.size() == 1 && fields[0] == "running")
        {
            state.running = true;
            return true;
        }
        // the number, then the 6 arguments in a syscall; sp and pc are always the last two
        if (fields.size() != 3 && fields.size() != 9)
        {
            return false;
        }
        if (fields.size() == 3 && fields[0] != "-1")
        {
            return false;
        }
        unsigned long long values[2] = {};
        for (int i = 0; i < 2; ++i)
        {
            const std::string& text = fields[fields.size() - 2 + i];
            char* end = nullptr;
            values[i] = std::strtoull(text.c_str(), &end, 16);
            if (text.compare(0, 2, "0x") != 0 || !end || *end)
            {
                return false;
            }
        }
        state.sp = values[0];
        state.pc = values[1];
        return true;
    }

    const int g_threadsCount = 1;
    CProcessSystem::CProcessSystem()
    {
        m_fsImpl = CreateDefaultProcessProvider();
        m_pool.Start(g_threadsCount);
    }

    // id-based stuff
    void CProcessSystem::AsyncStartQueryProcess(ThreadPtr_type targetThread,
        const ProcessUnifiedId& fileId,
        ProcessRecipientHandler_type openHandler,
        OperationPtr_type<QueryProcessHandler_type> filterHandler,
        int flags)
    {
        if (!targetThread)
        {
            return;
        }
        m_pool.AddTask([=,
            filterHandler = std::move(filterHandler),
            openHandler = std::move(openHandler)]() {

            m_fsImpl->AsyncStartQueryProcess(targetThread, fileId, openHandler, filterHandler, flags);
        });
    }

    std::tuple<int, std::shared_ptr<IProcess>> CProcessSystem::SyncOpenProcess(const oui::ProcessUnifiedId& procId)
    {
        return m_fsImpl->SyncOpenProcess(procId);
    }

    std::tuple<int, String> CProcessSystem::SyncQueryProcessName(unsigned long long pid)
    {
        return m_fsImpl->SyncQueryProcessName(pid);
    }

    std::tuple<int, ProcessStartInfo> CProcessSystem::SyncQueryProcessStartInfo(unsigned long long pid)
    {
        return m_fsImpl->SyncQueryProcessStartInfo(pid);
    }
}