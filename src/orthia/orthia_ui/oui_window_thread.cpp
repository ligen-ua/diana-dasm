#include "oui_window_thread.h"

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
    void CWindowThread::AddTask(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> guard(m_taskLock);
            m_tasks.push_back(task);
        }
        WakeUpUI();
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
            task();
        }
        m_uiBuffer.clear();
    }

}