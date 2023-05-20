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
        void WakeUpUI();
    };


    class BaseOperation:public Noncopyable
    {
    protected:
        std::atomic_bool m_cancelled = false;
    public:
        BaseOperation()
        {
        }
        virtual ~BaseOperation()
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
    };
    template<class HandlerType>
    class Operation:public BaseOperation
    {
        std::shared_ptr<CWindowThread> m_thread;
        HandlerType m_handler;

    public:
        template<class Type>
        Operation(std::shared_ptr<CWindowThread> thread, Type&& value)
            :
                m_thread(thread),
                m_handler(std::forward<Type>(value))
        {
        }
        template<class... Args>
        bool Reply(Args&&... args)
        {
            if (IsCancelled())
                return false;

            if (!m_thread)
                return false;

            auto params = std::make_tuple(std::forward<Args>(args)...);

            m_thread->AddTask([=, params = std::move(params)]() {
                std::apply(m_handler, params);
            });
            return true;
        }
        template<class... Args>
        bool ReplyWithRetain(std::shared_ptr<BaseOperation> opToRetain, Args&&... args)
        {
            if (IsCancelled())
                return false;

            if (!m_thread)
                return false;

            auto params = std::make_tuple(std::forward<Args>(args)...);

            m_thread->AddTask([=, opToRetain = opToRetain, params = std::move(params)]() {
                std::apply(m_handler, params);
            });
            return true;
        }

        HandlerType& GetHandler()
        {
            return m_handler;
        }
        const HandlerType& GetHandler() const
        {
            return m_handler;
        }

        template<class Type>
        void SetHandler(Type&& value)
        {
            m_handler = std::forward<Type>(value);
        }
    };

    template<class T>
    using OperationPtr_type = std::shared_ptr<Operation<T>>;
}