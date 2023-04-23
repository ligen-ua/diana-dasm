#pragma once
#include "oui_string.h"

namespace oui
{

class Param
{
    String m_param;
public:
    Param();
    Param(const String::char_type* pData, int size);
    Param(long value);
    Param(int value);
    Param(unsigned long value);
    Param(unsigned long long value);
    Param(long long value);
    Param(bool value);
    Param(const wchar_t * pValue);
    Param(const char * pValue);

    String ToString() const;
};

}