#pragma once

#include "oui_base.h"

namespace oui
{
    class CWindowThread:Noncopyable
    {
        std::mutex m_handlerLock;
        std::function<void()> m_addTaskHandler;

        std::mutex m_taskLock;
        std::vector<std::function<void()>> m_tasks;

        std::vector<std::function<void()>> m_uiBuffer;
    public:
        CWindowThread(std::function<void()> addTaskHandler = nullptr);
        void SetWakeupHandler(std::function<void()> addTaskHandler);
        void AddTask(std::function<void()> task);
        void GUI_ProcessTasks();
    };


    class BaseOperation:public Noncopyable
    {
    public:
    };
    template<class HandlerType>
    class Operation:public BaseOperation
    {
        HandlerType m_handler;
        std::atomic_bool m_cancelled = false;
    public:
        template<class Type>
        Operation(Type&& value)
            :
                m_handler(std::forward<Type>(value))
        {
        }
        void Cancel()
        {
            m_cancelled = true;
        }
        bool IsCancelled() const
        {
            return m_cancelled;
        }

        template<class... Args>
        bool Reply(Args&&... args)
        {
            if (IsCancelled())
                return false;
            m_handler(std::forward<Args>(args)...);
            return true;
        }
    };

    template<class T>
    using OperationPtr_type = std::shared_ptr<Operation<T>>;
}