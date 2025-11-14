#define _CRT_SECURE_NO_WARNINGS
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

void CLogOverWCOUT::LogData(const wchar_t * pData, ULONG size)
{
    std::wcout.write(pData, size);
}
log_meta::LogEncoding_type CLogOverWCOUT::QueryEncoding() const
{
    return log_meta::leUtf16LE;
}

CFileLog::CFileLog()
    :  
        m_encoding(log_meta::leUnknown), 
        m_bEnabled(false),
        m_usedCP(0)
{
}
CFileLog::CFileLog(const std::wstring & fileName, 
                   log_meta::LogEncoding_type encoding)
    :  
        m_encoding(encoding),
        m_usedCP(0)
{
            
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


    HANDLE hFile = 
        CreateFile(fileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_DELETE, 0, CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, 0);
    if (hFile == INVALID_HANDLE_VALUE)
        ORTHIA_THROW_WIN32("CFileLog.CannotCreateLog.CreateFile");
    m_file.Reset(hFile);
    
    m_bEnabled = true;

    if (m_encoding == log_meta::leUtf16LE)
    {
        DWORD written = 0;
        unsigned char bytes[] = {0xFF, 0xFE};
        if (!WriteFile(hFile, bytes, 2, &written, 0))
        {
            ORTHIA_THROW_WIN32("CFileLog.CannotCreateLog.WriteFile");
        }
    }
}
    
CFileLog::~CFileLog()
{
}
log_meta::LogEncoding_type CFileLog::QueryEncoding() const
{
    return m_encoding;
}
void CFileLog::LogData(const wchar_t * pData, ULONG size)
{
    if (!size)
    {
        return;
    }
    DWORD written = 0;
    if (m_encoding == log_meta::leUtf16LE)
    {
        WriteFile(m_file.Get(), pData, size*sizeof(wchar_t), &written, 0);
    }
    else
    {
        try
        {
            std::string result = orthia::ToAnsiString_Silent(std::wstring(pData, pData + size), m_usedCP);
            
            if (!result.empty())
            {
                WriteFile(m_file.Get(), 
                          result.c_str(), 
                          (ULONG)result.size(), 
                          &written, 
                          0);
            }
        }
        catch(...)
        {
        }
    }
}

CDebugOutputLog::CDebugOutputLog()
{

}
CDebugOutputLog::~CDebugOutputLog()
{
}
void CDebugOutputLog::LogData(const ORTHIA_TCHAR* pData, ULONG size)
{
    std::wstring text(pData, pData + size);
    if (text.empty()) 
    {
        return;
    }
    if (text.back() != 0xA) 
    {
        text.push_back(0xD);
        text.push_back(0xA);
    }
    OutputDebugStringW(text.c_str());
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
CLogParam::CLogParam(const wchar_t * pData, ULONG size)
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
        wcscpy(m_buf, L"[Out-of-memory]");
    }
}
CLogParam::CLogParam(const std::wstring & str)
    :
        m_type(lpString)
{
    m_buf[0] = 0;
    try
    {
        if (str.size() > g_iLogParamMaxBufSize)
            m_param = str;
        else
            wcscpy(m_buf, str.c_str());
           
    }
    catch(...)
    {
        wcscpy(m_buf, L"[Out-of-memory]");
    }
}
CLogParam::CLogParam(const std::string & str)
    :
        m_type(lpString)
{
    m_buf[0] = 0;
    try
    {
        std::wstring wstr = orthia::ToWideString(str);

        if (wstr.size() > g_iLogParamMaxBufSize)
            m_param = wstr;
        else
            wcscpy(m_buf, wstr.c_str());
    }
    catch(...)
    {
        wcscpy(m_buf, L"[Out-of-memory]");
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
CLogParam::CLogParam(const LARGE_INTEGER & value)
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
CLogParam::CLogParam(bool value)
    :
        m_type(lpBool)
{
    if (value)
        wcscpy(m_buf, L"true");
    else
        wcscpy(m_buf, L"false");
}
CLogParam::CLogParam(const wchar_t * pValue)
    :
        m_type(lpString)
{
    size_t size = wcslen(pValue);
    m_buf[0] = 0;
    try
    {
        if (size > g_iLogParamMaxBufSize)
            m_param.assign(pValue, pValue + size);
        else
            wcscpy(m_buf, pValue);
    }
    catch(...)
    {
        wcscpy(m_buf, L"[Out-of-memory]");
    }
}
CLogParam::CLogParam(const char * pValue)
    :
        m_type(lpString)
{
    m_buf[0] = 0;
    try
    {
        std::wstring wstr = orthia::ToWideString(pValue);

        if (wstr.size() > g_iLogParamMaxBufSize)
            m_param = wstr;
        else
            wcscpy(m_buf, wstr.c_str());
    }
    catch(...)
    {
        wcscpy(m_buf, L"[Out-of-memory]");
    }
}
bool CLogParam::IsSeverity(LogSeverity& severity) const
{
    if (m_type == lpSeverity)
    {
        severity = m_severity;
        return true;
    }
    return false;
}
const wchar_t * CLogParam::GetBegin() const
{
    if (m_buf[0])
        return m_buf;
    return m_param.c_str();
}
const wchar_t * CLogParam::GetEnd() const
{
    return GetBegin() + GetSize();
}
size_t CLogParam::GetSize() const
{
    if (m_buf[0])
        return wcslen(m_buf);
    return m_param.size();
}
std::wstring CLogParam::ToString() const
{
    if (m_buf[0])
        return m_buf;
    return m_param;
}
CLogParam::LogParam_type CLogParam::GetType() const
{
    return m_type;
}
void GenerateTimestamp(std::wstring * pTime)
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    wchar_t buffer[64];
    _snwprintf(buffer, sizeof(buffer)/sizeof(buffer[0]), 
        L"[%4i/%02i/%02i %02i:%02i:%02i:%03i]",
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
static std::atomic<LogSeverity> g_defSeverity = LogSeverity::Info;

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
