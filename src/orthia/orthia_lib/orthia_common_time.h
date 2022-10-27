#ifndef ORTHIA_COMMON_TIME_H
#define ORTHIA_COMMON_TIME_H

#include "orthia_utils.h"
#include "orthia_sqlite.h"

namespace orthia
{

class CSQLStringCache
{
    bool m_inited;
    SYSTEMTIME m_time;
    
public:
    CSQLStringCache();
    void LazyInit(const std::string & sqlTimeImpl);
    void Init(const SYSTEMTIME & time);
    void Clear();
    const SYSTEMTIME * ToSystemTime() const;
    std::wstring GUI_QueryConvertedToLocal() const;
    bool GUI_QueryConvertedToLocal(SYSTEMTIME * pTimeRes) const;
    std::wstring GUI_QueryStringJustDate() const;
};
class CCommonDateTime
{
    std::string m_sqlTimeImpl;
    mutable CSQLStringCache m_cache;
public:
    CCommonDateTime()
    {
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
    void InitFromSystemTime(const SYSTEMTIME & sqlValue)
    {
        m_sqlTimeImpl = orthia::ConvertSystemTimeToSQLite(sqlValue);
        m_cache.Init(sqlValue);
    }
    void InitFromSQL(const std::string & sqlValue)
    {
        m_sqlTimeImpl = sqlValue;
        m_cache.Clear();
    }
    std::string ToSQLTime() const
    {
        return m_sqlTimeImpl;
    }
    std::wstring GUI_QueryConvertedToLocal() const 
    {
        m_cache.LazyInit(m_sqlTimeImpl);
        return m_cache.GUI_QueryConvertedToLocal();
    }
    bool GUI_QueryConvertedToLocal(SYSTEMTIME * pTimeRes) const 
    {
        m_cache.LazyInit(m_sqlTimeImpl);
        return m_cache.GUI_QueryConvertedToLocal(pTimeRes);
    }
    std::wstring GUI_QueryStringJustDate() const
    {
        m_cache.LazyInit(m_sqlTimeImpl);
        return m_cache.GUI_QueryStringJustDate();
    }
    const SYSTEMTIME * ToSystemTime() const
    {
        m_cache.LazyInit(m_sqlTimeImpl);
        return m_cache.ToSystemTime();
    }
    long long ToLongLongTime() const
    {
        return orthia::ConvertSystemTimeToFileTime(ToSystemTime());
    }
};

}


#endif