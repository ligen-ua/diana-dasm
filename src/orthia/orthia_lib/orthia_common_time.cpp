#include "orthia_common_time.h"

namespace orthia
{

// Time
void CCommonDateTime::InitFromFileTime(long long utcTime)
{
    orthia::WinSystemTime_type st;
    orthia::ConvertFileTimeToSystemTime(utcTime, &st);
    InitFromSystemTime(st);
}
void CCommonDateTime::InitFromCurrentTime()
{
    orthia::WinSystemTime_type st;
    orthia::GetUtcTime(&st);
    InitFromSystemTime(st);
}
CSQLStringCache::CSQLStringCache()
    :
    m_inited(false)
{
    memset(&m_time, 0, sizeof(m_time));
}
void CSQLStringCache::Init(const orthia::WinSystemTime_type& time)
{
    m_time = time;
    m_inited = true;
}
void CSQLStringCache::LazyInit(const std::string& sqlTimeImpl)
{
    if (m_inited)
    {
        return;
    }
    m_inited = orthia::ConvertSQLTimeToSystemTime(sqlTimeImpl, &m_time);
}
void CSQLStringCache::Clear()
{
    m_inited = false;
    memset(&m_time, 0, sizeof(m_time));
}
const orthia::WinSystemTime_type* CSQLStringCache::ToSystemTime() const
{
    if (!m_inited)
        return 0;
    return &m_time;
}
bool CSQLStringCache::GUI_QueryConvertedToLocal(orthia::WinSystemTime_type* pTimeRes) const
{
    if (!m_inited)
    {
        return false;
    }
    orthia::WinSystemTime_type localTime = { 0, };
    if (!orthia::UtcTimeToLocal(m_time, &localTime))
    {
        return false;
    }
    *pTimeRes = localTime;
    return true;
}
orthia::PlatformString_type CSQLStringCache::GUI_QueryConvertedToLocal() const
{
    if (!m_inited)
    {
        return orthia::PlatformString_type();
    }

    orthia::WinSystemTime_type localTime = { 0, };
    if (!orthia::UtcTimeToLocal(m_time, &localTime))
    {
        return orthia::PlatformString_type();
    }

    return orthia::SystemTimeToString(localTime);
}
orthia::PlatformString_type CSQLStringCache::GUI_QueryStringJustDate() const
{
    if (!m_inited)
    {
        return orthia::PlatformString_type();
    }

    return orthia::SystemTimeToStringJustDate(m_time);
}
orthia::PlatformString_type CSQLStringCache::Android_QueryStringJustDate() const
{
    if (!m_inited)
    {
        return orthia::PlatformString_type();
    }

    return orthia::SystemTimeToStringJustDate(m_time);
}

}