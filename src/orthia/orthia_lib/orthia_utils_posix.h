#ifdef DIANA_HAS_POSIX

#include <mutex>
#include <thread>

namespace orthia
{

class CCriticalSection
{
    std::recursive_mutex m_section;
    CCriticalSection(const CCriticalSection&);
    CCriticalSection&operator = (const CCriticalSection&);

public:
    void Lock()
    {
        m_section.lock();
    }
    void Unlock()
    {
        m_section.unlock();
    }
    CCriticalSection()
    {
    }
    ~CCriticalSection()
    {
    }
};


orthia::PlatformString_type Downcase(const orthia::PlatformString_type & str);
std::string Downcase_Ansi(const std::string & str);

inline std::string Utf8ToPlatformString(const std::string & str) { return str; }
inline std::string PlatformStringToUtf8(const std::string & str) { return str; }
inline std::string ExpandVariable(const std::string & str) { return str; }

bool IsFileExist(const std::string & fileName);

}

#endif