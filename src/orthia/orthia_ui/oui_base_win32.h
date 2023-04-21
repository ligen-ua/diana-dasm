#pragma once
#include <windows.h>
#undef max
#undef min

namespace oui
{
    enum class EventType 
    { 
        Auto,
        Manual
    };

    class CEvent:Noncopyable
    {
        HANDLE m_hEvent;
    public:
        CEvent(EventType type)
        {
            m_hEvent = ::CreateEventW(NULL, type == EventType::Manual ? TRUE : FALSE, FALSE, NULL);
            if (m_hEvent == NULL)
                throw std::runtime_error("Can't create event.");
        }
        ~CEvent()
        {
            if (m_hEvent)
                CloseHandle(m_hEvent);
        }

        HANDLE GetHandle()
        {
            return m_hEvent;
        }
        void Set()
        {
            ::SetEvent(m_hEvent);
        }
        void Reset()
        {
            ::ResetEvent(m_hEvent);
        }
        bool Wait(DWORD dwTimeInMs = INFINITE)
        {
            DWORD dwRes = WaitForSingleObject(m_hEvent, dwTimeInMs);
            if (dwRes == WAIT_TIMEOUT)
                return false;
            if (dwRes == WAIT_OBJECT_0)
                return true;
            return false;
        }
    };


}