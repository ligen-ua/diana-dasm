#pragma once

#include "orthia_utils.h"
#include "orthia_files.h"

namespace orthia
{
    void GenerateTimestamp(orthia::PlatformString_type * pTime);

    enum class LogSeverity
    {
        Alert,
        Critical,
        Error,
        Warning,
        Info,
        Debug
    };

    PlatformString_type SeverityNameToString();
    PlatformString_type SeverityValueToString(LogSeverity severity);

    struct log_meta
    {
        typedef enum {leUnknown, leUtf16LE, leUtf8, leANSI} LogEncoding_type;
    };
    struct ILowLevelLog:public orthia::IRefCountedBase
    {
        virtual ~ILowLevelLog(){}
        virtual void LogData(const ORTHIA_TCHAR * pData, int size)=0;
        virtual log_meta::LogEncoding_type QueryEncoding() const=0;
    };

    inline void LogData(orthia::intrusive_ptr<ILowLevelLog> pLog,
                        const ORTHIA_TCHAR * pData)
    {
        pLog->LogData(pData, (int)ORTHIA_TLEN(pData));
    }

#ifdef WIN32
    class CLogOverWCOUT:public RefCountedBase_t<ILowLevelLog>
    {
    public:
        virtual void LogData(const ORTHIA_TCHAR * pData, int size);
        virtual log_meta::LogEncoding_type QueryEncoding() const;
    };
#endif

    template<class StreamType>
    class CLogOverCmdStream:public RefCountedBase_t<ILowLevelLog>
    {
        StreamType m_stream;
    public:
        virtual void LogData(const ORTHIA_TCHAR * pData, int size)
        {
            m_stream.Write(orthia::PlatformString_type(pData, pData + size));
        }
        virtual log_meta::LogEncoding_type QueryEncoding() const
        {
#ifdef WIN32 
            return log_meta::leUtf16LE;
#else
            return log_meta::leUtf8;
#endif
        }
    };
    class CFileLog:public RefCountedBase_t<ILowLevelLog>
    {
        orthia::CFile m_file;
        log_meta::LogEncoding_type m_encoding;
        bool m_bEnabled;
        int m_usedCP;
    public:
        CFileLog(const orthia::PlatformString_type & fileName, 
                 log_meta::LogEncoding_type encoding);
        CFileLog();
        ~CFileLog();
        virtual void LogData(const ORTHIA_TCHAR * pData, int size);
        virtual log_meta::LogEncoding_type QueryEncoding() const;
    };
    class CDebugOutputLog :public RefCountedBase_t<ILowLevelLog>
    {
    public:
        CDebugOutputLog();
        ~CDebugOutputLog();
        virtual void LogData(const ORTHIA_TCHAR* pData, int size);
        virtual log_meta::LogEncoding_type QueryEncoding() const;
    };
    const int g_iLogParamMaxBufSize = 63;

    class CLogParam
    {
    public:
        typedef enum {lpNone, lpString, lpInt, lpBool, lpResource, lpSeverity} LogParam_type;

    private:
        union {
            ORTHIA_TCHAR m_buf[g_iLogParamMaxBufSize + 1];
            LogSeverity m_severity;
        };
        orthia::PlatformString_type m_param;
        orthia::PlatformString_type m_name;
        LogParam_type m_type;
    public:
        CLogParam();
        CLogParam(LogSeverity severity);
        CLogParam(const ORTHIA_TCHAR * pData, int size);
        CLogParam(const orthia::PlatformString_type & str);
#ifdef WIN32
        CLogParam(const std::string & str);
        CLogParam(const char * pValue);
#endif
        CLogParam(long value, long radix = 10);
        CLogParam(unsigned long value, long radix = 10);
        CLogParam(unsigned long long value, long radix = 10);
        CLogParam(const orthia::LargeInteger_type & value);
        CLogParam(long long value);
        CLogParam(bool value);
        CLogParam(const ORTHIA_TCHAR * pValue);
  
        bool IsSeverity(LogSeverity & severity) const;
        const ORTHIA_TCHAR * GetBegin() const;
        const ORTHIA_TCHAR * GetEnd() const;
        size_t GetSize() const;
        orthia::PlatformString_type ToString() const; 
        LogParam_type GetType() const; 
        CLogParam & SetName(const orthia::PlatformString_type & name) { m_name = name; return *this; } 
        const orthia::PlatformString_type & GetName() const { return m_name; }
    };

    template<class Type> 
    struct LogHelper
    {
        Type m_value;
        long m_radix;
    public:
        LogHelper(Type value, long radix)
            : m_value(value), m_radix(radix)
        {
        }
    };

    template<class Type> 
    inline LogHelper<Type> Hex(const Type & obj) { return LogHelper<Type>(obj, 16); }

    class CLogParamEx:public CLogParam
    {
    public:
        CLogParamEx() {} 
        CLogParamEx(LogSeverity severity) : CLogParam(severity) {   }
        CLogParamEx(const ORTHIA_TCHAR * pData, int size) : CLogParam(pData, size)  {   }
        CLogParamEx(const orthia::PlatformString_type & str) : CLogParam(str)  {   }
        CLogParamEx(long value, long radix = 10) : CLogParam(value, radix)  {   }
        CLogParamEx(unsigned long value, long radix = 10) : CLogParam(value, radix)  {   }
        CLogParamEx(const orthia::LargeInteger_type & value) : CLogParam(value)  {   }
        CLogParamEx(long long value) : CLogParam(value)  {   }
        CLogParamEx(unsigned long long value, long radix = 10) : CLogParam(value, radix)  {   }
        CLogParamEx(bool value) : CLogParam(value)  {   }
        CLogParamEx(const ORTHIA_TCHAR * pValue) : CLogParam(pValue)  {   }

#ifdef WIN32
        CLogParamEx(const std::string & str) : CLogParam(str)  {   }
        CLogParamEx(const char * pValue) : CLogParam(pValue)  {   }
#endif

        CLogParamEx(const LogHelper<long> & obj) : CLogParam(obj.m_value, obj.m_radix)  {   }
        CLogParamEx(const LogHelper<unsigned long> & obj) : CLogParam(obj.m_value, obj.m_radix)  {   }
        CLogParamEx(const LogHelper<unsigned long long> & obj) : CLogParam(obj.m_value, obj.m_radix)  {   }

        CLogParamEx& SetName(const orthia::PlatformString_type & name) { CLogParam::SetName(name); return *this; } 
    };
        


    struct ILog:public orthia::IRefCountedBase
    {
        virtual void LogNewLine(orthia::PlatformString_type& data) = 0;

        virtual void AppendSeverity(orthia::PlatformString_type& data, 
                                    LogSeverity severity) = 0;

        virtual void AppendParameter(orthia::PlatformString_type & data,
                                     const CLogParamEx & param)=0;
        virtual void AppendNamedParameter(orthia::PlatformString_type & data,
                                     const CLogParamEx & param)=0;
        virtual void AppendTableColumn(orthia::PlatformString_type & data,
                                     const CLogParamEx & param)=0;
        virtual void AppendTableCell(orthia::PlatformString_type & data,
                                     const CLogParamEx & param)=0;
        virtual void StartTextArea(orthia::PlatformString_type & data)=0;
        virtual void DoneTextArea(orthia::PlatformString_type & data)=0;
        virtual void StartTable(orthia::PlatformString_type & data)=0;
        virtual void DoneHeader(orthia::PlatformString_type & data)=0;
        virtual void DoneTable(orthia::PlatformString_type & data)=0;
        virtual void Finalize(orthia::PlatformString_type & data)=0;
        virtual void StartTableRow(orthia::PlatformString_type & data)=0;
        virtual void DoneTableRow(orthia::PlatformString_type & data)=0;
    };
    struct CBaseLog:ILog
    {
        virtual PlatformString_type SeverityNameToString() 
        {
            return orthia::SeverityNameToString();
        }
        virtual PlatformString_type SeverityValueToString(LogSeverity severity)
        {
            return orthia::SeverityValueToString(severity);
        }

        virtual void AppendNamedParameter(orthia::PlatformString_type & data,
                                     const CLogParamEx & param)
        {
            AppendParameter(data, param);
            LogNewLine(data);
        }
        virtual void AppendTableColumn(orthia::PlatformString_type & data,
                                     const CLogParamEx & param)
        {
            AppendParameter(data, param);
        }
        virtual void AppendTableCell(orthia::PlatformString_type & data,
                                     const CLogParamEx & param)
        {
            AppendParameter(data, param);
        }
        virtual void StartTextArea(orthia::PlatformString_type & data) {}
        virtual void DoneTextArea(orthia::PlatformString_type & data) 
        {
            LogNewLine(data);
        }
        virtual void StartTable(orthia::PlatformString_type & data) {}
        virtual void DoneHeader(orthia::PlatformString_type & data)
        {
            LogNewLine(data);
        }
        virtual void DoneTable(orthia::PlatformString_type & data) 
        {
            LogNewLine(data);
        }
        virtual void Finalize(orthia::PlatformString_type & data) {}
        virtual void StartTableRow(orthia::PlatformString_type & data) {}
        virtual void DoneTableRow(orthia::PlatformString_type & data) 
        {
            LogNewLine(data);
        }
    };
    class CGenericLog:public RefCountedBase_t<CBaseLog>
    {
        orthia::intrusive_ptr<ILowLevelLog> m_pLog;
    public:
        CGenericLog(orthia::intrusive_ptr<ILowLevelLog> pLog)
            :
                m_pLog(pLog)
        {
        }
        void AppendSeverity(orthia::PlatformString_type& data,
            LogSeverity severity)
        {
            AppendParameter(data,
                this->SeverityValueToString(severity) + ORTHIA_TCSTR(" "));
        }
        virtual void AppendParameter(orthia::PlatformString_type & data,
                                     const CLogParamEx & param)
        {
            const ORTHIA_TCHAR * pData = param.GetBegin();
            size_t size = param.GetSize();
            data.append(pData, pData + size);
        }
        virtual void LogNewLine(orthia::PlatformString_type & data)
        {
            data.append(ORTHIA_TCSTR("\n"));
            m_pLog->LogData(data.c_str(), (int)data.size());
        }
    };

    template<class Type>
    class log_time:public Type
    {
    public:
        virtual void LogNewLine(orthia::PlatformString_type & data)
        {
            orthia::PlatformString_type stampedData;
            GenerateTimestamp(&stampedData);
            stampedData.append(data);
            stampedData.append(ORTHIA_TCSTR(" "));
            stampedData.append(data);
            Type::LogNewLine(stampedData);
        }
    };
    template<class Type>
    class log_sync:public Type
    {
        orthia::CCriticalSection m_lock;
    public:
        virtual void LogNewLine(orthia::PlatformString_type & data)
        {
            orthia::CAutoCriticalSection guard(m_lock);
            Type::LogNewLine(data);
        }
    };

    class CProgramLog:public CGenericLog
    {
        orthia::intrusive_ptr<ILowLevelLog> m_pLog;
        bool m_generateTimeStamp;
    public:
        CProgramLog(orthia::intrusive_ptr<ILowLevelLog> pLog, bool generateTimeStamp = true);
        void AppendSeverity(orthia::PlatformString_type& data,
            LogSeverity severity) override;
    };

    class CLogWrapper
    {
        ILog * m_pLog;
        orthia::PlatformString_type m_data;
    public:
        CLogWrapper(ILog * pLog);
        ~CLogWrapper();
        void LogData(const CLogParamEx & param);
    };

    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, const CLogParamEx & param)
    {
        if (!pLog)
            return;
        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param);
    }
    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, const CLogParamEx & param1, const CLogParamEx & param2)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
    }
    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, 
                         const CLogParamEx & param1, 
                         const CLogParamEx & param2,
                         const CLogParamEx & param3)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
        logWrapper.LogData(param3);
    }    
    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, 
                         const CLogParamEx & param1, 
                         const CLogParamEx & param2,
                         const CLogParamEx & param3,
                         const CLogParamEx & param4)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
        logWrapper.LogData(param3);
        logWrapper.LogData(param4);
    }
    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, 
                         const CLogParamEx & param1, 
                         const CLogParamEx & param2,
                         const CLogParamEx & param3,
                         const CLogParamEx & param4,
                         const CLogParamEx & param5)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
        logWrapper.LogData(param3);
        logWrapper.LogData(param4);
        logWrapper.LogData(param5);
    }
    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, 
                         const CLogParamEx & param1, 
                         const CLogParamEx & param2,
                         const CLogParamEx & param3,
                         const CLogParamEx & param4,
                         const CLogParamEx & param5,
                         const CLogParamEx & param6)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
        logWrapper.LogData(param3);
        logWrapper.LogData(param4);
        logWrapper.LogData(param5);
        logWrapper.LogData(param6);
    }

    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, 
                         const CLogParamEx & param1, 
                         const CLogParamEx & param2,
                         const CLogParamEx & param3,
                         const CLogParamEx & param4,
                         const CLogParamEx & param5,
                         const CLogParamEx & param6,
                         const CLogParamEx & param7)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
        logWrapper.LogData(param3);
        logWrapper.LogData(param4);
        logWrapper.LogData(param5);
        logWrapper.LogData(param6);
        logWrapper.LogData(param7);
    }

    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, 
                         const CLogParamEx & param1, 
                         const CLogParamEx & param2,
                         const CLogParamEx & param3,
                         const CLogParamEx & param4,
                         const CLogParamEx & param5,
                         const CLogParamEx & param6,
                         const CLogParamEx & param7,
                         const CLogParamEx & param8)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
        logWrapper.LogData(param3);
        logWrapper.LogData(param4);
        logWrapper.LogData(param5);
        logWrapper.LogData(param6);
        logWrapper.LogData(param7);
        logWrapper.LogData(param8);
    }

    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, 
                         const CLogParamEx & param1, 
                         const CLogParamEx & param2,
                         const CLogParamEx & param3,
                         const CLogParamEx & param4,
                         const CLogParamEx & param5,
                         const CLogParamEx & param6,
                         const CLogParamEx & param7,
                         const CLogParamEx & param8,
                         const CLogParamEx & param9)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
        logWrapper.LogData(param3);
        logWrapper.LogData(param4);
        logWrapper.LogData(param5);
        logWrapper.LogData(param6);
        logWrapper.LogData(param7);
        logWrapper.LogData(param8);
        logWrapper.LogData(param9);
    }

    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, 
                         const CLogParamEx & param1, 
                         const CLogParamEx & param2,
                         const CLogParamEx & param3,
                         const CLogParamEx & param4,
                         const CLogParamEx & param5,
                         const CLogParamEx & param6,
                         const CLogParamEx & param7,
                         const CLogParamEx & param8,
                         const CLogParamEx & param9,
                         const CLogParamEx & param10)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
        logWrapper.LogData(param3);
        logWrapper.LogData(param4);
        logWrapper.LogData(param5);
        logWrapper.LogData(param6);
        logWrapper.LogData(param7);
        logWrapper.LogData(param8);
        logWrapper.LogData(param9);
        logWrapper.LogData(param10);
    }

    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog, 
                         const CLogParamEx & param1, 
                         const CLogParamEx & param2,
                         const CLogParamEx & param3,
                         const CLogParamEx & param4,
                         const CLogParamEx & param5,
                         const CLogParamEx & param6,
                         const CLogParamEx & param7,
                         const CLogParamEx & param8,
                         const CLogParamEx & param9,
                         const CLogParamEx & param10,
                         const CLogParamEx & param11)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
        logWrapper.LogData(param3);
        logWrapper.LogData(param4);
        logWrapper.LogData(param5);
        logWrapper.LogData(param6);
        logWrapper.LogData(param7);
        logWrapper.LogData(param8);
        logWrapper.LogData(param9);
        logWrapper.LogData(param10);
        logWrapper.LogData(param11);
    }


    inline void WriteLog(orthia::intrusive_ptr<ILog> pLog,
        const CLogParamEx& param1,
        const CLogParamEx& param2,
        const CLogParamEx& param3,
        const CLogParamEx& param4,
        const CLogParamEx& param5,
        const CLogParamEx& param6,
        const CLogParamEx& param7,
        const CLogParamEx& param8,
        const CLogParamEx& param9,
        const CLogParamEx& param10,
        const CLogParamEx& param11,
        const CLogParamEx& param12)
    {
        if (!pLog)
            return;

        CLogWrapper logWrapper(pLog.get());
        logWrapper.LogData(param1);
        logWrapper.LogData(param2);
        logWrapper.LogData(param3);
        logWrapper.LogData(param4);
        logWrapper.LogData(param5);
        logWrapper.LogData(param6);
        logWrapper.LogData(param7);
        logWrapper.LogData(param8);
        logWrapper.LogData(param9);
        logWrapper.LogData(param10);
        logWrapper.LogData(param11);
        logWrapper.LogData(param12);
    }


    LogSeverity DefLog_GetLogSeverity();
    void DefLog_SetLogSeverity(LogSeverity severity);

    void DefLog_Init(orthia::intrusive_ptr<ILog> pLog);
    void DefLog_Reset();
    void DefLog_Release();
    orthia::intrusive_ptr<ILog> DefLog_Get();


    inline void DefLog_Log(LogSeverity severity,
        const CLogParamEx& param)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;
        WriteLog(pLog, severity, param);
    }
    inline void DefLog_Log(LogSeverity severity, 
        const CLogParamEx& param1, const CLogParamEx& param2)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;

        WriteLog(pLog, severity, param1, param2);
    }
    inline void DefLog_Log(LogSeverity severity,
        const CLogParamEx& param1,
        const CLogParamEx& param2,
        const CLogParamEx& param3)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;

        WriteLog(pLog, severity, param1, param2, param3);
    }
    inline void DefLog_Log(LogSeverity severity,
        const CLogParamEx& param1,
        const CLogParamEx& param2,
        const CLogParamEx& param3,
        const CLogParamEx& param4)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;

        WriteLog(pLog, severity, param1, param2, param3, param4);
    }
    inline void DefLog_Log(LogSeverity severity,
        const CLogParamEx& param1,
        const CLogParamEx& param2,
        const CLogParamEx& param3,
        const CLogParamEx& param4,
        const CLogParamEx& param5)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;

        WriteLog(pLog, severity, param1, param2, param3, param4, param5);
    }
    inline void DefLog_Log(LogSeverity severity,
        const CLogParamEx& param1,
        const CLogParamEx& param2,
        const CLogParamEx& param3,
        const CLogParamEx& param4,
        const CLogParamEx& param5,
        const CLogParamEx& param6)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;

        WriteLog(pLog, severity, param1, param2, param3, param4, param5, param6);
    }

    inline void DefLog_Log(LogSeverity severity,
        const CLogParamEx& param1,
        const CLogParamEx& param2,
        const CLogParamEx& param3,
        const CLogParamEx& param4,
        const CLogParamEx& param5,
        const CLogParamEx& param6,
        const CLogParamEx& param7)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;

        WriteLog(pLog, severity, param1, param2, param3, param4, param5, param6, param7);
    }

    inline void DefLog_Log(LogSeverity severity,
        const CLogParamEx& param1,
        const CLogParamEx& param2,
        const CLogParamEx& param3,
        const CLogParamEx& param4,
        const CLogParamEx& param5,
        const CLogParamEx& param6,
        const CLogParamEx& param7,
        const CLogParamEx& param8)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;

        WriteLog(pLog, severity, param1, param2, param3, param4, param5, param6, param7, param8);
    }

    inline void DefLog_Log(LogSeverity severity,
        const CLogParamEx& param1,
        const CLogParamEx& param2,
        const CLogParamEx& param3,
        const CLogParamEx& param4,
        const CLogParamEx& param5,
        const CLogParamEx& param6,
        const CLogParamEx& param7,
        const CLogParamEx& param8,
        const CLogParamEx& param9)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;

        WriteLog(pLog, severity, param1, param2, param3, param4, param5, param6, param7, param8, param9);
    }

    inline void DefLog_Log(LogSeverity severity,
        const CLogParamEx& param1,
        const CLogParamEx& param2,
        const CLogParamEx& param3,
        const CLogParamEx& param4,
        const CLogParamEx& param5,
        const CLogParamEx& param6,
        const CLogParamEx& param7,
        const CLogParamEx& param8,
        const CLogParamEx& param9,
        const CLogParamEx& param10)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;

        WriteLog(pLog, severity, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
    }

    inline void DefLog_Log(LogSeverity severity,
        const CLogParamEx& param1,
        const CLogParamEx& param2,
        const CLogParamEx& param3,
        const CLogParamEx& param4,
        const CLogParamEx& param5,
        const CLogParamEx& param6,
        const CLogParamEx& param7,
        const CLogParamEx& param8,
        const CLogParamEx& param9,
        const CLogParamEx& param10,
        const CLogParamEx& param11)
    {
        orthia::intrusive_ptr<ILog> pLog = DefLog_Get();
        if (!pLog || severity > DefLog_GetLogSeverity())
            return;

        WriteLog(pLog, severity, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11);
    }

    
#define ORTHIA_LOG orthia::DefLog_Log


#ifdef _DEBUG
#define ORTHIA_DEV_LOG orthia::DefLog_Log
#else 
#define ORTHIA_DEV_LOG(...)
#endif


}
