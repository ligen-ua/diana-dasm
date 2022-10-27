#include "orthia_common_time.h"

namespace orthia
{


// Time
void CCommonDateTime::InitFromCurrentTime()
{
    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    GetSystemTime(&st);
    InitFromSystemTime(st);
}
CSQLStringCache::CSQLStringCache()
    :
        m_inited(false)
{
    memset(&m_time, 0, sizeof(m_time));
}
void CSQLStringCache::Init(const SYSTEMTIME & time)
{
    m_time = time;
    m_inited = true;
}
void CSQLStringCache::LazyInit(const std::string & sqlTimeImpl)
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
const SYSTEMTIME * CSQLStringCache::ToSystemTime() const
{
    if (!m_inited)
        return 0;
    return &m_time;
}
bool CSQLStringCache::GUI_QueryConvertedToLocal(SYSTEMTIME * pTimeRes) const
{
    if (!m_inited)
    {
        return false;
    }
    SYSTEMTIME localTime = {0,};
    if (!SystemTimeToTzSpecificLocalTime(0, &m_time, &localTime))
    {
        return false;
    }
    *pTimeRes = localTime;
    return true;
}
std::wstring CSQLStringCache::GUI_QueryConvertedToLocal() const
{
    if (!m_inited)
    {
        return std::wstring();
    }

    SYSTEMTIME localTime = {0,};
    if (!SystemTimeToTzSpecificLocalTime(0, &m_time, &localTime))
    {
        return std::wstring();
    }

    return orthia::SystemTimeToWideString(localTime);
}
std::wstring CSQLStringCache::GUI_QueryStringJustDate() const
{
    if (!m_inited)
    {
        return std::wstring();
    }

    return orthia::SystemTimeToWideStringJustDate(m_time);
}

}