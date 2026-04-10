#include "orthia_log.h"
#include "iostream"
#include "atomic"
namespace orthia
{



PlatformString_type SeverityNameToString() 
{
    return ORTHIA_TCSTR("Severity");
}
PlatformString_type SeverityValueToString(LogSeverity severity)
{
    switch (severity)
    {
    case LogSeverity::Alert:
        return ORTHIA_TCSTR("Alert");

    case LogSeverity::Critical:
        return ORTHIA_TCSTR("Critical");

    case LogSeverity::Error:
        return ORTHIA_TCSTR("Error");

    case LogSeverity::Warning:
        return ORTHIA_TCSTR("Warning");

    case LogSeverity::Info:
        return ORTHIA_TCSTR("Info");

    case LogSeverity::Debug:
        return ORTHIA_TCSTR("Debug");

    default:
        return ORTHIA_TCSTR("Unknown");
    }
}

#ifdef WIN32

void CLogOverWCOUT::LogData(const ORTHIA_TCHAR * pData, int size)
{
    std::wcout.write(pData, size);
}
log_meta::LogEncoding_type CLogOverWCOUT::QueryEncoding() const
{
    return log_meta::leUtf16LE;
}

#endif

CFileLog::CFileLog()
    :  
        m_encoding(log_meta::leUnknown), 
        m_bEnabled(false),
        m_usedCP(0)
{
}
CFileLog::CFileLog(const orthia::PlatformString_type & fileName, 
                   log_meta::LogEncoding_type encoding)
    :  
        m_encoding(encoding),
        m_usedCP(0)
{

#ifdef WIN32 
    switch(m_encoding)
    {
    case log_meta::leUtf16LE:
        break;
    case log_meta::leUtf8:
        m_usedCP = CP_UTF8;
        break;
    case log_meta::leANSI:
        m_usedCP = CP_ACP;
        break;
    default:
        throw std::runtime_error("Can't support encoding: " + orthia::ObjectToString_Ansi((int)m_encoding));
    }
#else
    switch(m_encoding)
    {
    case log_meta::leUtf8:
        break;
    default:
        throw std::runtime_error("Can't support encoding: " + orthia::ObjectToString_Ansi((int)m_encoding));
    }
#endif

    m_file.CreateNewAlways(fileName);    
    m_bEnabled = true;

    if (m_encoding == log_meta::leUtf16LE)
    {
        unsigned char bytes[] = {0xFF, 0xFE};
        m_file.WriteToFile(bytes, 2);
    }
}
    
CFileLog::~CFileLog()
{
}
log_meta::LogEncoding_type CFileLog::QueryEncoding() const
{
    return m_encoding;
}
void CFileLog::LogData(const ORTHIA_TCHAR * pData, int size)
{
    if (!size)
    {
        return;
    }
    
#ifdef WIN32
    if (m_encoding == log_meta::leUtf16LE)
    {
        m_file.WriteToFile(pData, size*sizeof(ORTHIA_TCHAR));
    }
    else
    {
        try
        {
            std::string result = orthia::ToAnsiString_Silent(orthia::PlatformString_type(pData, pData + size), m_usedCP);
            
            if (!result.empty())
            {
                m_file.WriteToFile(result.c_str(),
                    (int)result.size());
            }
        }
        catch(...)
        {
        }
    }
#else
    m_file.WriteToFile(pData, size);
#endif

}

CDebugOutputLog::CDebugOutputLog()
{

}
CDebugOutputLog::~CDebugOutputLog()
{
}
void CDebugOutputLog::LogData(const ORTHIA_TCHAR* pData, int size)
{
    orthia::PlatformString_type text(pData, pData + size);
    if (text.empty()) 
    {
        return;
    }
    if (text.back() != 0xA) 
    {
#ifdef WIN32
        text.push_back(0xD);
#endif
        text.push_back(0xA);
    }
#ifdef WIN32
    OutputDebugStringW(text.c_str());
#else
    fprintf(stderr, "%s", text.c_str());
#endif
}
log_meta::LogEncoding_type CDebugOutputLog::QueryEncoding() const
{
    return log_meta::leUtf16LE;
}

// CLogParam's
CLogParam::CLogParam()
    :
        m_type(lpNone)
{
}

CLogParam::CLogParam(LogSeverity severity)
    :
        m_type(lpSeverity)
{
    m_severity = severity;
}
CLogParam::CLogParam(const ORTHIA_TCHAR * pData, int size)
    :
        m_type(lpString)
{
    m_buf[0] = 0;
    try
    {
        if (size > g_iLogParamMaxBufSize)
            m_param.assign(pData, pData + size);
    }
    catch(...)
    {
        ORTHIA_TSTRCPY(m_buf, ORTHIA_TCSTR("[Out-of-memory]"));
    }
}
CLogParam::CLogParam(const orthia::PlatformString_type & str)
    :
        m_type(lpString)
{
    m_buf[0] = 0;
    try
    {
        if (str.size() > g_iLogParamMaxBufSize)
            m_param = str;
        else
            ORTHIA_TSTRCPY(m_buf, str.c_str());
           
    }
    catch(...)
    {
        ORTHIA_TSTRCPY(m_buf, ORTHIA_TCSTR("[Out-of-memory]"));
    }
}
#ifdef WIN32
CLogParam::CLogParam(const std::string & str)
    :
        m_type(lpString)
{
    m_buf[0] = 0;
    try
    {
        orthia::PlatformString_type wstr = orthia::ToWideString(str);

        if (wstr.size() > g_iLogParamMaxBufSize)
            m_param = wstr;
        else
            ORTHIA_TSTRCPY(m_buf, wstr.c_str());
    }
    catch(...)
    {
        ORTHIA_TSTRCPY(m_buf, ORTHIA_TCSTR("[Out-of-memory]"));
    }
}

CLogParam::CLogParam(unsigned long long value, long radix)
    :
        m_type(lpInt)
{
    _ui64tow(value, m_buf, radix);
}
CLogParam::CLogParam(long value, long radix)
    :
        m_type(lpInt)
{
    _ltow(value, m_buf, radix);
}
CLogParam::CLogParam(unsigned long value, long radix)
    :
        m_type(lpInt)
{
    _ultow(value, m_buf, radix);
}
CLogParam::CLogParam(const orthia::LargeInteger_type & value)
    :
        m_type(lpInt)
{
    _i64tow(value.QuadPart, m_buf, 10);
}
CLogParam::CLogParam(long long value)
    :
        m_type(lpInt)
{
    _i64tow(value, m_buf, 10);
}

#else

int portable_ui64toa(uint64_t value, char* buffer, size_t size, int radix);

CLogParam::CLogParam(unsigned long long value, long radix)
    :
        m_type(lpInt)
{
    portable_ui64toa(value, m_buf, sizeof(m_buf)-1, radix);
}
CLogParam::CLogParam(long value, long radix)
    :
        m_type(lpInt)
{
    if (radix == 10 && value < 0) 
    {
        m_buf[0] = '-';
        portable_ui64toa(-value, m_buf+1, sizeof(m_buf)-2, radix);
        return;
    }
    portable_ui64toa(value, m_buf, sizeof(m_buf)-1, radix);
}
CLogParam::CLogParam(unsigned long value, long radix)
    :
        m_type(lpInt)
{
    portable_ui64toa(value, m_buf, sizeof(m_buf)-1, radix);
}
CLogParam::CLogParam(const orthia::LargeInteger_type & value)
    :
        m_type(lpInt)
{
    portable_ui64toa(value.QuadPart, m_buf, sizeof(m_buf)-1, 10);
}
CLogParam::CLogParam(long long value)
    :
        m_type(lpInt)
{
    if (value < 0) 
    {
        m_buf[0] = '-';
        portable_ui64toa(-value, m_buf+1, sizeof(m_buf)-2, 10);
        return;
    }
    portable_ui64toa(value, m_buf, sizeof(m_buf)-1, 10);
}


#endif


CLogParam::CLogParam(bool value)
    :
        m_type(lpBool)
{
    if (value)
        ORTHIA_TSTRCPY(m_buf, ORTHIA_TCSTR("true"));
    else
        ORTHIA_TSTRCPY(m_buf, ORTHIA_TCSTR("false"));
}
CLogParam::CLogParam(const ORTHIA_TCHAR * pValue)
    :
        m_type(lpString)
{
    size_t size = ORTHIA_TLEN(pValue);
    m_buf[0] = 0;
    try
    {
        if (size > g_iLogParamMaxBufSize)
            m_param.assign(pValue, pValue + size);
        else
            ORTHIA_TSTRCPY(m_buf, pValue);
    }
    catch(...)
    {
        ORTHIA_TSTRCPY(m_buf, ORTHIA_TCSTR("[Out-of-memory]"));
    }
}

#ifdef WIN32
CLogParam::CLogParam(const char * pValue)
    :
        m_type(lpString)
{
    m_buf[0] = 0;
    try
    {
        orthia::PlatformString_type wstr = orthia::ToWideString(pValue);

        if (wstr.size() > g_iLogParamMaxBufSize)
            m_param = wstr;
        else
            ORTHIA_TSTRCPY(m_buf, wstr.c_str());
    }
    catch(...)
    {
        ORTHIA_TSTRCPY(m_buf, ORTHIA_TCSTR("[Out-of-memory]"));
    }
}
#endif

bool CLogParam::IsSeverity(LogSeverity& severity) const
{
    if (m_type == lpSeverity)
    {
        severity = m_severity;
        return true;
    }
    return false;
}
const ORTHIA_TCHAR * CLogParam::GetBegin() const
{
    if (m_buf[0])
        return m_buf;
    return m_param.c_str();
}
const ORTHIA_TCHAR * CLogParam::GetEnd() const
{
    return GetBegin() + GetSize();
}
size_t CLogParam::GetSize() const
{
    if (m_buf[0])
        return ORTHIA_TLEN(m_buf);
    return m_param.size();
}
orthia::PlatformString_type CLogParam::ToString() const
{
    if (m_buf[0])
        return m_buf;
    return m_param;
}
CLogParam::LogParam_type CLogParam::GetType() const
{
    return m_type;
}
void GenerateTimestamp(orthia::PlatformString_type * pTime)
{
    orthia::WinSystemTime_type st;
    GetUtcTime(&st);
    ORTHIA_TCHAR buffer[64];
    ORTHIA_SNTPRINTF(buffer, sizeof(buffer)/sizeof(buffer[0]), 
        ORTHIA_TCSTR("[%4i/%02i/%02i %02i:%02i:%02i:%03i]"),
        (int)st.wYear,
        (int)st.wMonth,
        (int)st.wDay,
        (int)st.wHour,
        (int)st.wMinute,
        (int)st.wSecond,
        (int)st.wMilliseconds);
    
    pTime->assign(buffer);
}

CLogWrapper::CLogWrapper(ILog * pLog)
    : m_pLog(pLog)
{
    try
    {
        m_data.reserve(1024);
    }
    catch(...)
    {
    }
}
CLogWrapper::~CLogWrapper()
{
    try
    {
        m_pLog->LogNewLine(m_data);
    }
    catch(...)
    {
    }
}
void CLogWrapper::LogData(const CLogParamEx & param)
{
    try
    {
        LogSeverity severity;
        if (param.IsSeverity(severity)) 
        {
            m_pLog->AppendSeverity(m_data, severity);
            return;
        }
        m_pLog->AppendParameter(m_data, param);
    }
    catch(...)
    {
    }
}

// default log support
static orthia::intrusive_ptr<ILog> g_pDefLog;
static std::atomic<LogSeverity> g_defSeverity =
#ifdef _DEBUG 
    LogSeverity::Debug;
#else
    LogSeverity::Info;
#endif

LogSeverity DefLog_GetLogSeverity()
{
    return g_defSeverity;
}
void DefLog_SetLogSeverity(LogSeverity severity)
{
    g_defSeverity = severity;
}

void DefLog_Init(orthia::intrusive_ptr<ILog> pLog)
{
    g_pDefLog = 0;
    g_pDefLog = pLog;
}
void DefLog_Reset()
{
    g_pDefLog = 0;
}
void DefLog_Release()
{
    if (g_pDefLog.get())
    {
        g_pDefLog->OrthiaAddRef();
    }
    g_pDefLog = 0;
}
orthia::intrusive_ptr<ILog> DefLog_Get()
{
    return g_pDefLog;
}

// log 
CProgramLog::CProgramLog(orthia::intrusive_ptr<ILowLevelLog> pLog, bool generateTimeStamp)
    :
    CGenericLog(pLog),
    m_generateTimeStamp(generateTimeStamp)
{
}
void CProgramLog::AppendSeverity(orthia::PlatformString_type& data,
    LogSeverity severity)
{
    orthia::PlatformString_type stampedData;
    if (m_generateTimeStamp)
    {
        GenerateTimestamp(&stampedData);
        stampedData += ORTHIA_TCSTR(" ");

        stampedData += this->SeverityValueToString(severity);
        stampedData += ORTHIA_TCSTR(": ");
    }
    else
    {
        stampedData += ORTHIA_TCSTR("[");
        stampedData += this->SeverityValueToString(severity);
        stampedData += ORTHIA_TCSTR("] ");
    }
    data += stampedData;
}

}
