#include "oui_threadpool.h"

namespace oui
{

    CThreadPool::CThreadPool(size_t threadCount)
    {
        Start(threadCount);
    }

    CThreadPool::~CThreadPool()
    {
        NotifyStop();
        JoinAll();
    }
    void CThreadPool::Start(size_t threadCount)
    {
        for (size_t i = 0; i < threadCount; ++i)
        {
            AddThread();
        }
    }
    void CThreadPool::AddThread()
    {
        std::lock_guard<std::mutex> guard(m_threadsLock);
        m_threads.push_back(std::make_shared<std::thread>([this]() { MainLoop();  }));
    }
    void CThreadPool::AddTask(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(m_tasksLock);
            m_tasks.emplace_back(std::move(task));
        }
        m_tasksCondition.notify_all();
    }

    void CThreadPool::MainLoop()
    {
        for (; !m_stopped;)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_tasksLock);
                if (m_tasks.empty())
                {
                    m_tasksCondition.wait(lock);
                    continue;
                }
                task = std::move(m_tasks.front());
                m_tasks.pop_front();
            }

            try
            {
                task();
            }
            catch (const std::exception& e)
            {
                oui::LogOutput(LogFlags::Error, e.what());
            }
        }
    }

    void CThreadPool::NotifyStop()
    {
        m_stopped.store(true);
        m_tasksCondition.notify_all();
    }

    void CThreadPool::JoinAll()
    {
        std::lock_guard<std::mutex> guard(m_threadsLock);

        for (auto thread:m_threads)
        {
            if (thread->joinable())
            {
                thread->join();
            }
        }
    }

}