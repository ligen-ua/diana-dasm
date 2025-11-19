#pragma once
#include "oui_base.h"

namespace oui
{
    class CThreadPool:Noncopyable
    {
        std::atomic<bool> m_stopped = false;

        std::mutex m_threadsLock;
        std::list<std::shared_ptr<std::thread>> m_threads;


        // task management
        std::condition_variable m_tasksCondition;
        std::mutex m_tasksLock;
        std::list<std::function<void()>> m_tasks;
        
        void MainLoop();
        void NotifyStop();
        void JoinAll();
    public:
        CThreadPool(size_t threadCount = 0);
        ~CThreadPool();

        void Start(size_t threadCount);
        void AddThread();
        void AddTask(std::function<void()> task);
    };
}


