#ifndef ORTHIA_COMMON_TIME_H
#define ORTHIA_COMMON_TIME_H

#include "orthia_utils.h"
#include "orthia_sqlite.h"

namespace orthia
{

class CSQLStringCache
{
    bool m_inited;
    orthia::WinSystemTime_type m_time;

public:
    CSQLStringCache();
    void LazyInit(const std::string& sqlTimeImpl);
    void Init(const orthia::WinSystemTime_type& time);
    void Clear();
    const orthia::WinSystemTime_type* ToSystemTime() const;
    orthia::PlatformString_type GUI_QueryConvertedToLocal() const;
    bool GUI_QueryConvertedToLocal(orthia::WinSystemTime_type* pTimeRes) const;
    orthia::PlatformString_type GUI_QueryStringJustDate() const;
    orthia::PlatformString_type Android_QueryStringJustDate() const;
};


class CCommonDateTime
{
    std::string m_sqlTimeImpl;
    mutable CSQLStringCache m_cache;
public:
    CCommonDateTime()
    {
    }
    CCommonDateTime(const orthia::WinSystemTime_type& sqlValue)
    {
        InitFromSystemTime(sqlValue);
    }
    void Clear()
    {
        m_sqlTimeImpl.clear();
        m_cache.Clear();
    }
    bool IsEmpty() const
    {
        return m_sqlTimeImpl.empty();
    }
    void InitFromCurrentTime();
    void InitFromFileTime(long long fileTime);
    void InitFromUnixTime(long long unixSeconds);
    void InitFromSystemTime(const orthia::WinSystemTime_type& sqlValue)
    {
        m_sqlTimeImpl = orthia::ConvertSystemTimeToSQLite(sqlValue);
        m_cache.Init(sqlValue);
    }
    void InitFromSQL(const std::string& sqlValue)
    {
        m_sqlTimeImpl = sqlValue;
        m_cache.Clear();
    }
    std::string ToSQLTime() const
    {
        return m_sqlTimeImpl;
    }
    orthia::PlatformString_type GUI_QueryConvertedToLocal() const
    {
        m_cache.LazyInit(m_sqlTimeImpl);
        return m_cache.GUI_QueryConvertedToLocal();
    }
    bool GUI_QueryConvertedToLocal(orthia::WinSystemTime_type* pTimeRes) const
    {
        m_cache.LazyInit(m_sqlTimeImpl);
        return m_cache.GUI_QueryConvertedToLocal(pTimeRes);
    }
    orthia::PlatformString_type GUI_QueryStringJustDate() const
    {
        m_cache.LazyInit(m_sqlTimeImpl);
        return m_cache.GUI_QueryStringJustDate();
    }
    orthia::PlatformString_type Android_QueryStringJustDate() const
    {
        m_cache.LazyInit(m_sqlTimeImpl);
        return m_cache.Android_QueryStringJustDate();
    }
    const orthia::WinSystemTime_type* ToSystemTime() const
    {
        m_cache.LazyInit(m_sqlTimeImpl);
        return m_cache.ToSystemTime();
    }
    long long ToLongLongTime() const
    {
        const orthia::WinSystemTime_type* pSystemTime = ToSystemTime();
        if (!pSystemTime)
        {
            return 0;
        }
        return orthia::ConvertSystemTimeToFileTime(pSystemTime);
    }
};


}


#endif