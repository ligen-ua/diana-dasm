#ifdef OUI_SYS_POSIX

#pragma once

#include "assert.h"

#define OUI_DEBUG_BREAK   assert(0)
#define OUI_INFINITE      ((unsigned int)-1)
#define OUI_TLEN   strlen

namespace oui
{
    std::string Uppercase_Silent(const std::string& str);
    void FilterUnreadableSymbols(std::string& text);
    bool StartsWith(const std::string& text, const std::string& phrase);

    class CEvent:Noncopyable
    {
        mutable std::mutex m_lock;
        std::condition_variable m_variable;
        bool m_state;
        EventType m_type;

        CEvent(const CEvent &);
        CEvent&operator =(const CEvent &);

    public:
        CEvent(EventType type)
            :
                m_state(false),
                m_type(type)
        {
        }
        ~CEvent()
        {
        }
        void Set()
        {
            std::unique_lock<std::mutex> lock(m_lock);
            m_state = true;
            if (m_type == EventType::Auto)
            {
                m_variable.notify_one();
            }
            else
            {
                m_variable.notify_all();
            }
        }
        void Reset()
        {
            std::unique_lock<std::mutex> lock(m_lock);
            m_state = false;
        }
        bool Wait(unsigned int msToWait = OUI_INFINITE)
        {
            std::unique_lock<std::mutex> lock(m_lock);
            if (bool res = m_state)
            {
                if (m_type == EventType::Auto) {
                    m_state = false;
                }
                return res;
            }
            if (msToWait == OUI_INFINITE)
            {
                m_variable.wait(lock,
                        [&]
                        {
                            bool res = m_state;
                            if (m_type == EventType::Auto)
                            {
                                m_state = false;
                            }
                            return res;
                        });
                return true;
            }
            return m_variable.wait_for(lock,
                            std::chrono::milliseconds(msToWait),
                            [&] {
                                        bool res = m_state;
                                        if (m_type == EventType::Auto)
                                        {
                                            m_state = false;
                                        }
                                        return res;
                                }
                            );
        }
        bool ReadState() const
        {
            std::unique_lock<std::mutex> lock(m_lock);
            return m_state;
        }
    };


    inline std::string GetErrorText(int dwError)
    {
        std::stringstream res;
        res<<"Error, code "<<dwError;
        return res.str();
    }


    void LogOutput(LogFlags flags, const std::string& text);
    void LogOutput(LogFlags flags, const std::wstring& text);


}

#endif
