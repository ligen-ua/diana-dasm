#include "oui_window_thread.h"
#include <algorithm>

namespace oui
{
    CWindowThread::CWindowThread(std::function<void()> addTaskHandler)
        :
        m_addTaskHandler(addTaskHandler)
    {
    }
    void CWindowThread::SetWakeupHandler(std::function<void()> addTaskHandler)
    {
        std::lock_guard<std::mutex> guard(m_handlerLock);
        m_addTaskHandler = std::move(addTaskHandler);
    }
    void CWindowThread::RemoveTask(SharedFunction_type task)
    {
        std::lock_guard<std::mutex> guard(m_taskLock);
        m_tasks.erase(std::remove(m_tasks.begin(), m_tasks.end(), task), m_tasks.end());
    }
    CWindowThread::SharedFunction_type CWindowThread::AddTask(std::function<void()> task)
    {
        auto ptask = std::make_shared<std::function<void()>>(task);
        {
            std::lock_guard<std::mutex> guard(m_taskLock);
            m_tasks.push_back(ptask);
        }
        WakeUpUI();
        return ptask;
    }
    void CWindowThread::WakeUpUI()
    {
        std::lock_guard<std::mutex> guard(m_handlerLock);
        if (m_addTaskHandler)
        {
            m_addTaskHandler();
        }
    }
    void CWindowThread::GUI_ProcessTasks()
    {
        {
            std::lock_guard<std::mutex> guard(m_taskLock);
            m_uiBuffer = m_tasks;
            m_tasks.clear();
        }
        for (auto task : m_uiBuffer)
        {
            try
            {
                (*task)();
            }
            catch (std::exception& e)
            {
                oui::LogOutput(LogFlags::Error, e.what());
            }
        }
        m_uiBuffer.clear();
    }

}