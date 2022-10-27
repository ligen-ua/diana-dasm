#pragma once 


namespace orthia_shuttle
{


#pragma pack(push, 1)
struct PrintArgument
{
    typedef enum {paNone, paPointer, paAnsi, paWide, paInt32, paInt64, paDouble, paBool}  Argument_type;

    static const int flags_hex = 1;
    static const int flags_signed = 2;
    Argument_type type;
    int flags;

    union
    {
        unsigned long long int64Data;
        unsigned int int32Data;
        double doubleData;
    };
};
#pragma pack(pop)

inline void InitArgument(PrintArgument & arg, 
                         PrintArgument::Argument_type type,
                         int flags)
{
    for(char * p = (char * )&arg, * p_end = (char *)(&arg+1); p != p_end; ++p)
    {
        *p = 0;
    }
    arg.type = type;
    arg.flags = flags;
}
typedef long (__stdcall * PrintStream_type)(const PrintArgument * pText, int count);
struct IHypervisorInterface
{
    PrintStream_type printStream;
};

class CPrintStream
{
    static const int m_maxBufferSize = 0x10;
    PrintArgument m_buffer[m_maxBufferSize];

    int m_sizeUsed;
    int m_currentFlags;
    IHypervisorInterface * m_pHypervisorInterface;
public:
    CPrintStream(IHypervisorInterface * pHypervisorInterface)
        :
            m_pHypervisorInterface(pHypervisorInterface),
            m_sizeUsed(0),
            m_currentFlags(0)
    {
    }

    ~CPrintStream()
    {
        Flush();
    }

    void Flush()
    {
        if (m_sizeUsed)
        {
            PrintStream_type printStream = m_pHypervisorInterface->printStream;
            printStream(m_buffer, m_sizeUsed);
            m_sizeUsed = 0;
        }
    }
    void SetHex()
    {
        m_currentFlags |= PrintArgument::flags_hex;
    }
    void SetDec()
    {
        m_currentFlags &= ~PrintArgument::flags_hex;
    }
    void Append(const PrintArgument & arg)
    {
        if (m_sizeUsed >= m_maxBufferSize)
        {
            Flush();
        }
        if (m_sizeUsed < m_maxBufferSize)
        {
            m_buffer[m_sizeUsed] = arg;
            ++m_sizeUsed;
        }
    }
    CPrintStream & operator << (const char * pAnsiData)
    {
        PrintArgument arg;
        InitArgument(arg, PrintArgument::paAnsi, m_currentFlags);
        arg.int64Data = (unsigned long long )pAnsiData;
        Append(arg);
        Flush();
        return *this;
    }
    CPrintStream & operator << (const wchar_t * pWideData)
    {
        PrintArgument arg;
        InitArgument(arg, PrintArgument::paWide, m_currentFlags);
        arg.int64Data = (unsigned long long )pWideData;
        Append(arg);
        Flush();
        return *this;
    }
    CPrintStream & operator << (unsigned int value)
    {
        PrintArgument arg;
        InitArgument(arg, PrintArgument::paInt32, m_currentFlags);
        arg.int32Data = value;
        Append(arg);
        return *this;
    }
    CPrintStream & operator << (unsigned long long value)
    {
        PrintArgument arg;
        InitArgument(arg, PrintArgument::paInt64, m_currentFlags);
        arg.int64Data = value;
        Append(arg);
        return *this;
    }
    CPrintStream & operator << (double value)
    {
        PrintArgument arg;
        InitArgument(arg, PrintArgument::paDouble, m_currentFlags);
        arg.doubleData = value;
        Append(arg);
        return *this;
    }
    CPrintStream & operator << (int value)
    {
        PrintArgument arg;
        InitArgument(arg, PrintArgument::paInt32, m_currentFlags|PrintArgument::flags_signed);
        arg.int32Data = (unsigned int)value;
        Append(arg);
        return *this;
    }
    CPrintStream & operator << (long long value)
    {
        PrintArgument arg;
        InitArgument(arg, PrintArgument::paInt64, m_currentFlags|PrintArgument::flags_signed);
        arg.int64Data = (unsigned long long)value;
        Append(arg);
        return *this;
    }
    CPrintStream & operator << (bool value)
    {
        PrintArgument arg;
        InitArgument(arg, PrintArgument::paBool, m_currentFlags);
        arg.int32Data = (int)value;
        Append(arg);
        return *this;
    }
    CPrintStream & operator << (const void * pValue)
    {
        PrintArgument arg;
        InitArgument(arg, PrintArgument::paPointer, PrintArgument::flags_hex);
        arg.int64Data = (unsigned long long )pValue;
        Append(arg);
        return *this;
    }
};

#pragma pack(push, 1)
struct ShuttleArgument
{
    int interfaceVersion;
    int reserved;
    IHypervisorInterface * pHypervisorInterface;
    int argsCount;
    int headerSize;

    void * GetArgument(int number)
    {
        if (number >= argsCount)
        {
            return 0;
        }
        return ((void**)((char*)this+headerSize))[number];
    }
    unsigned long long GetArgument_Long(int number)
    {
        return (unsigned long long)GetArgument(number);
    }
};
#pragma pack(pop)

#define ORTHIA_SHUTTLE_EXPORT(X)  extern "C" void __stdcall X(void * pDllAddress, unsigned long , orthia_shuttle::ShuttleArgument & arguments)

#define ORTHIA_SHUTTLE_EXPORT2(X)  extern "C" __declspec( dllexport ) void __stdcall X(void * pDllAddress, unsigned long , orthia_shuttle::ShuttleArgument & arguments)
#define ORTHIA_FORCE_EXPORT comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)




}