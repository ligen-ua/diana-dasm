#include "orthia_vmlib_shuttle.h"
#include "orthia_vmlib_api_handlers.h"

#include "orthia_shuttle_interface.h"
namespace orthia
{

const int g_hypervisorInterfaceFunctionsCount = 1;

#pragma pack(push, 1)
struct RawHypervisorInterface64
{
    DI_UINT64 pPrintStream;
};
struct RawShuttleArgument64
{
    int interfaceVersion;
    int reserved;
    DI_UINT64 pHypervisorInterface;
    int argsCount;
    int headerSize;
};
struct RawHypervisorInterface32
{
    DI_UINT32 pPrintStream;
};
struct RawShuttleArgument32
{
    int interfaceVersion;
    int reserved;
    DI_UINT32 pHypervisorInterface;
    int argsCount;
    int headerSize;
};
#pragma pack(pop)


// Structure:
// [       Header                 ]
// [Shuttle][HyperVisor][API Traps][Args]

template<class ArgType, class HyperType, class OffsetType>
int InitRawShuttleArgument(std::vector<char> & buffer, 
                            int * pArgsCountOffset,
                            OffsetType offset)
{
    buffer.resize(sizeof(ArgType) + sizeof(HyperType) + g_hypervisorInterfaceFunctionsCount);
    ArgType * pArg = (ArgType*)&buffer.front();
    pArg->interfaceVersion = 1;
    pArg->headerSize = (int)(buffer.size());
    pArg->pHypervisorInterface = offset + sizeof(ArgType);
    *pArgsCountOffset = (int)((char*)&pArg->argsCount - (char*)pArg);

    HyperType * pHyperInterface = (HyperType*)&buffer[sizeof(ArgType)];
    int trapStartOffset = sizeof(ArgType) + sizeof(HyperType);
    int currentTrap = trapStartOffset;
    // functions stubs
    pHyperInterface->pPrintStream = offset + currentTrap++;
    return trapStartOffset;
}


CShuttleArgsBuilder::CShuttleArgsBuilder(Address_type offset, 
                                         int dianaMode)
    :
        m_dianaMode(dianaMode),
        m_argsCountOffset(0),
        m_trapStartOffset(0)
{
    switch(m_dianaMode)
    {
    default:
        throw std::runtime_error("Invalid mode");
    case DIANA_MODE32:
        m_trapStartOffset = InitRawShuttleArgument<RawShuttleArgument32, RawHypervisorInterface32, DI_UINT32>
                               (m_buffer, 
                                &m_argsCountOffset,
                               (DI_UINT32)offset);
        break;
    case DIANA_MODE64:;
        m_trapStartOffset = InitRawShuttleArgument<RawShuttleArgument64, RawHypervisorInterface64, DI_UINT64>
                               (m_buffer, 
                                &m_argsCountOffset,
                                offset);
        break;
    }
}
void CShuttleArgsBuilder::AddArgument(Address_type arg)
{
    m_buffer.insert(m_buffer.end(), (const char*)&arg, (const char*)&arg + m_dianaMode);
    ++(*(int*)&m_buffer[m_argsCountOffset]);
}
void CShuttleArgsBuilder::Produce(PrebuiltShuttleArgument * pArg)
{
    pArg->buffer = m_buffer;
    pArg->trapStartOffset = m_trapStartOffset;
    pArg->functionsCount = g_hypervisorInterfaceFunctionsCount;

}

//--------------------
class CShuttlePrintStream 
{
    CommonHandlerParameters & m_parameters;
    std::wstring m_tmp;
    std::string m_tmpAnsi;
    std::wstringstream m_res;
    std::vector<char> m_buffer;
public:
    CShuttlePrintStream(CommonHandlerParameters & parameters)
        :
            m_parameters(parameters)
    {
    }
    void ReadAnsi(OPERAND_SIZE dataAddress)
    {
        DianaProcessor * pCallContext = m_parameters.pProcessor->GetSelf();
        m_buffer.resize(256);
        OPERAND_SIZE readBytes = 0;
        DI_CHECK_CPP(DianaProcessor_ReadMemory(m_parameters.pProcessor->GetSelf(), 
                              GET_REG_DS,
                              dataAddress,
                              orthia::GetFrontPointer(m_buffer),
                              m_buffer.size(),
                              &readBytes,
                              0,
                              reg_DS));
        m_buffer[m_buffer.size()-1] = 0;
        m_tmpAnsi = &m_buffer.front();
    }
    void ReadWide(OPERAND_SIZE dataAddress)
    {
        DianaProcessor * pCallContext = m_parameters.pProcessor->GetSelf();
        m_buffer.resize(256*2);
        OPERAND_SIZE readBytes = 0;
        DI_CHECK_CPP(DianaProcessor_ReadMemory(m_parameters.pProcessor->GetSelf(), 
                              GET_REG_DS,
                              dataAddress,
                              orthia::GetFrontPointer(m_buffer),
                              m_buffer.size(),
                              &readBytes,
                              0,
                              reg_DS));
        m_buffer[m_buffer.size()-2] = 0;
        m_buffer[m_buffer.size()-1] = 0;
        m_tmp = (wchar_t*)&m_buffer.front();
      
    }
    void Flush()
    {
        m_tmp = m_res.str();
        m_parameters.pDebugInterface->Print(m_tmp);
    }
    CShuttlePrintStream & operator << (const orthia_shuttle::PrintArgument & op)
    {
        switch(op.type)
        {
        case orthia_shuttle::PrintArgument::paPointer: 
            if (m_parameters.dianaMode == DIANA_MODE64)
            {
                orthia::ToStringAsHex(op.int64Data, &m_tmp);
            }
            else
            {
                orthia::ToStringAsHex((DI_UINT32)op.int64Data, &m_tmp);
            }
            m_res<<m_tmp;
            break;
        case orthia_shuttle::PrintArgument::paAnsi: 
            ReadAnsi(op.int64Data);
            m_res<<orthia::ToWideString(m_tmpAnsi);
            break;
        case orthia_shuttle::PrintArgument::paWide:
            ReadWide(op.int64Data);
            m_res<<m_tmp;
            break;
        case orthia_shuttle::PrintArgument::paInt32:
            if (op.flags & op.flags_hex)
            {
                orthia::ToStringAsHex(op.int32Data, &m_tmp);
                m_res<<m_tmp;
                break;
            }
            if (op.flags & op.flags_signed)
            {
                m_res<<(DI_INT32)op.int32Data;
                break;
            }
            m_res<<op.int32Data;
            break;
        case orthia_shuttle::PrintArgument::paInt64:
            if (op.flags & op.flags_hex)
            {
                orthia::ToStringAsHex_Short(op.int64Data, &m_tmp);
                m_res<<m_tmp;
                break;
            }
            if (op.flags & op.flags_signed)
            {
                m_res<<(DI_INT64)op.int64Data;
                break;
            }
            m_res<<op.int32Data;
            break;
        case orthia_shuttle::PrintArgument::paBool:
            if (op.int32Data)
            {
                m_res<<"true";
                break;
            }
            m_res<<"false";
            break;

        case orthia_shuttle::PrintArgument::paDouble:
            m_res<<op.doubleData;
            break;

        case orthia_shuttle::PrintArgument::paNone:
        default:
            break;
        }
        return *this;
    }
};
// API handlers
static bool API_Handler_PrintStream(CommonHandlerParameters & parameters)
{
    OPERAND_SIZE args[3];
    DianaProcessor * pCallContext = parameters.pProcessor->GetSelf();

    DI_CHECK_CPP(QueryStdHookArgs(parameters.pProcessor, 0, args, 3));

    DI_UINT32 printArgsCount = (DI_UINT32)args[2];
    std::vector<char> buffer;
    buffer.resize(printArgsCount * sizeof(orthia_shuttle::PrintArgument));
    DI_CHECK_CPP(DianaProcessor_ReadMemory_Exact(parameters.pProcessor->GetSelf(), 
                              GET_REG_DS,
                              args[1],
                              orthia::GetFrontPointer(buffer),
                              buffer.size(),
                              0,
                              reg_DS));

    CShuttlePrintStream res(parameters);
    for(orthia_shuttle::PrintArgument * p = (orthia_shuttle::PrintArgument * )orthia::GetFrontPointer(buffer), * p_end = p + printArgsCount;
        p != p_end;
        ++p)
    {
        res<<*p;
    }
    res.Flush();
    DI_CHECK_CPP(SkipStdFunctionCall(parameters.pProcessor, args, 3, 0));
    return true;
}
CShuttleAPIHandlerPopulator::CShuttleAPIHandlerPopulator()
{
}
void CShuttleAPIHandlerPopulator::RegisterHandlers(ICommonAPIHandlerStorage * pCommonAPIHandlerStorage,
                                                   Address_type argumentsOffset)
{
    Address_type currentArgumentsOffset = argumentsOffset;
    pCommonAPIHandlerStorage->RegisterHandler(currentArgumentsOffset++,  API_Handler_PrintStream);
}


}