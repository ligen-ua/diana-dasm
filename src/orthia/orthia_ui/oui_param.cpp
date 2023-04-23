#include "oui_param.h"

namespace oui
{

Param::Param()
{
}
Param::Param(const String::char_type* pData, int size)
{
    try
    {
        if (size < 0)
            size = (int)wcslen(pData);
        m_param.native.assign(pData, pData + size);
    }
    catch(const std::bad_alloc &)
    {
        m_param = OUI_STR("[Out-of-memory]");
    }
}
Param::Param(unsigned long long value)
{
    m_param.native = OUI_TO_STR(value);
}
Param::Param(long value)
{
    m_param.native = OUI_TO_STR(value);
}
Param::Param(int value)
{
    m_param.native = OUI_TO_STR(value);
}
Param::Param(unsigned long value)
{
    m_param.native = OUI_TO_STR(value);
}
Param::Param(long long value)
{
    m_param.native = OUI_TO_STR(value);
}
Param::Param(bool value)
{
    if (value)
        m_param = OUI_STR("Yes");
    else
        m_param = OUI_STR("No");
}
String Param::ToString() const
{
    return m_param;
}


}