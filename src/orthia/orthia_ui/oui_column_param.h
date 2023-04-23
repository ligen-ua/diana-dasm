#pragma once

#include "oui_param.h"

namespace oui
{
    enum class ColumnType 
    { 
        ctUnknown, 
        ctString, 
        ctInt, 
        ctDouble 
    };
    enum class ColumnFormat
    {
        ctUnknown,
        ctLeft,
        ctRight,
        ctCenter
    };
    class ColumnParam
    {
        String m_name;
        bool m_empty;
        int m_width;
        ColumnFormat m_format;
        ColumnType m_type;
        int m_flags;
    public:
        ColumnParam()
            :
            m_empty(true),
            m_type(ColumnType::ctUnknown),
            m_flags(0)
        {
        }
        ColumnParam(const String& name,
            int width = 100,
            ColumnFormat format = ColumnFormat::ctLeft,
            ColumnType type = ColumnType::ctString,
            int flags = 0)
            :
            m_name(name),
            m_empty(false),
            m_width(width),
            m_format(format),
            m_type(type),
            m_flags(flags)
        {
        }
        String GetName() const
        {
            return m_name;
        }
        bool IsEmpty() const
        {
            return m_empty;
        }
        ColumnFormat GetFormat() const
        {
            return m_format;
        }
        int GetWidth() const
        {
            return m_width;
        }
        ColumnType GetType() const
        {
            return m_type;
        }
        int GetFlags() const
        {
            return m_flags;
        }
    };

}