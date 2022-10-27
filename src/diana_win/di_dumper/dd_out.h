#pragma once

#include "iostream"
#include "sstream"
#include "string"

namespace dd
{

typedef enum {otError, otInfo, otDebug, otCOUNT} OutType_type; 

void AppendStream(std::ostream & ostream, std::string & data);
bool GetOutStatus(OutType_type type, std::string * pPrefix);

class base_out
{
    std::ostream & m_impl;

protected:
    base_out(std::ostream & impl)
        :
            m_impl(impl)
    {
    }
public:
    template<class Type>
    base_out & operator << (Type obj)
    {
        m_impl<<obj;
        return *this;
    }
    base_out & operator << (const std::wstring & data);
};



class line_out:public base_out
{
    std::stringstream m_line;
    std::ostream & m_resStream;
    bool m_enabled;
public:
    line_out(OutType_type outType, std::ostream & resStream)
        : 
            base_out(m_line),
            m_resStream(resStream)
    {
        std::string prefix;
        m_enabled = GetOutStatus(outType, &prefix);
        m_line<<prefix;
    }
    ~line_out()
    {
        if (m_enabled)
        {
            m_line<<"\n";
            AppendStream(m_resStream, m_line.str());
        }
    }
};


class error_out:public line_out
{
public:
    error_out()
        :
            line_out(otError, std::cerr)
    {
    }
};
class info_out:public line_out
{
public:
    info_out()
        :
            line_out(otInfo, std::cout)
    {
    }
};
class debug_out:public line_out
{
public:
    debug_out()
        :
            line_out(otDebug, std::cout)
    {
    }
};
void VerboseDebugOn();

}