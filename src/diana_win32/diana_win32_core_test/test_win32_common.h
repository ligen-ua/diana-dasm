#ifndef TEST_WIN32_COMMON_H
#define TEST_WIN32_COMMON_H

#pragma warning(disable:4996)

extern "C"
{
#include "diana_win32_processor.h"
#include "diana_processor/diana_processor_cmd_internal.h"
#include "diana_patchers.h"
#include "diana_win32_ehook.h"
}

#include "test_common.h"
#include "vector"
#include "sstream"

struct State
{
    DWORD m_Eip;
    DWORD m_ESP;
    DWORD m_EBP;
    DWORD m_EAX;

    DWORD m_EFlags;
 
    DWORD m_EDI;
    DWORD m_ESI;    
  
    DWORD m_EBX;
    DWORD m_EDX;
    DWORD m_ECX;    
    State(DianaProcessor * pCallContext)
    {
        m_EFlags = pCallContext->m_flags.impl.l.value;
 
        m_EDI = (DWORD)GET_REG_EDI;
        m_ESI = (DWORD)GET_REG_ESI;
        m_EBP = (DWORD)GET_REG_EBP;
        m_ESP = (DWORD)GET_REG_ESP;
        m_EBX = (DWORD)GET_REG_EBX;
        m_EDX = (DWORD)GET_REG_EDX;
        m_ECX = (DWORD)GET_REG_ECX;
        m_EAX = (DWORD)GET_REG_EAX;
    
        m_Eip = (DWORD)GET_REG_RIP;
    }
};

class CBuffer
{
    std::vector<char> buf;
    std::vector<char> tmp;

public:
    CBuffer()
    {
       buf.resize(1024*1024);
       tmp.resize(1024);
    }
    inline CBuffer& operator <<(const char * pstr) 
    {
        size_t len = strlen(pstr);
        buf.insert(buf.end(), pstr, pstr +len);
        return *this;
    }
    inline CBuffer&operator <<(unsigned __int64 reg)  
    {
        char * pTmp = &tmp.front();
        _ultoa((unsigned long )reg, pTmp, 16);
        size_t len = strlen(pTmp);
        buf.insert(buf.end(), pTmp, pTmp +len);
        return *this;
    }
    void start()
    {
        buf.resize(0);
        tmp.resize(1024);
    }
    void flush()
    {
    }
    std::string str() const
    {
        return std::string(buf.begin(), buf.end());
    }
};
inline void PrintContext(DianaProcessor * pCallContext)
{

    static CBuffer str;

    str.start();
    //std::hex(str);
    str<<"[";
    str<<GET_REG_CS<<"-";
    str<<GET_REG_RIP<<"-";
    str<<GET_REG_EDI<<"-";
    str<<GET_REG_ESI<<"-";
    str<<GET_REG_EBX<<"-";
    str<<GET_REG_EDX<<"-";
    str<<GET_REG_ECX<<"-";
    str<<GET_REG_EAX<<"-";
    str<<GET_REG_EBP<<"-";
    str<<pCallContext->m_flags.impl.l.value<<"-";
    str<<GET_REG_ESP<<"-";
    str<<GET_REG_SS;
    str<<"]\n\n";
    str.flush();

    std::string str2 = str.str();
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), 
                  str2.c_str(), 
                  (DWORD)str2.size(), 
                  &written, 
                  0);

    /*std::hex(str);
    str<<"[";
    str<<GET_REG_CS<<"-";
    str<<GET_REG_RIP<<"-";
    str<<GET_REG_EDI<<"-";
    str<<GET_REG_ESI<<"-";
    str<<GET_REG_EBX<<"-";
    str<<GET_REG_EDX<<"-";
    str<<GET_REG_ECX<<"-";
    str<<GET_REG_EAX<<"-";
    str<<GET_REG_EBP<<"-";
    str<<pCallContext->m_flags.l.value<<"-";
    str<<GET_REG_ESP<<"-";
    str<<GET_REG_SS;
    str<<"]\n\n";
    str.flush();

    std::string str2 = str.str();
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), 
                  str2.c_str(), 
                  str2.size(), 
                  &written, 
                  0);*/
}

const int g_stackSize = 1024*1024;
extern char g_stack[g_stackSize];


#endif