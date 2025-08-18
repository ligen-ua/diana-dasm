#include "orthia_commands.h"
#include "orthia_dbgext.h"
#include "orthia_common_print.h"
#include "orthia.h"
#include "orthia_memory_cache.h"
namespace orthia
{

CWindbgMemoryReader::CWindbgMemoryReader(int defaultDianaMode)
    :
        m_defaultDianaMode(defaultDianaMode)
{
}
void CWindbgMemoryReader::Init(int defaultDianaMode)
{
    m_defaultDianaMode = defaultDianaMode;
}
void CWindbgMemoryReader::Read(orthia::Address_type offset, 
                              orthia::Address_type bytesToRead,
                              void * pBuffer,
                              orthia::Address_type * pBytesRead,
                              int flags,
                              orthia::Address_type selectorValue,
                              DianaUnifiedRegister selectorHint)
{
    if (bytesToRead >= (orthia::Address_type)ULONG_MAX)
    {
        std::stringstream text;
        text<<"Range size is too big: "<<bytesToRead;
        throw std::runtime_error(text.str());
    }
    if (selectorValue)
    {
        orthia::Orthia_CustomMemoryRead(offset,
                                        pBuffer,
                                        bytesToRead,
                                        pBytesRead,
                                        selectorHint);
        if (*pBytesRead == 0)
        {
            // try another way, sometimes windbg is a tough guy 
            if ((m_defaultDianaMode == DIANA_MODE64 && selectorHint == reg_GS)
                ||
                (m_defaultDianaMode == DIANA_MODE32 && selectorHint == reg_FS))
            {
                std::stringstream arg;
                std::hex(arg);
                arg<<"@$teb+0x"<<offset;
                Orthia_CustomMemoryReadEx(pBuffer,
                                        bytesToRead,
                                        pBytesRead,
                                        arg.str());
                return;
            }
        }
        return;
    }

    if (!(flags & ORTHIA_MR_FLAG_READ_THROUGH))
    {
        ULONG bytes = 0;
        ReadMemory(offset, 
                   pBuffer, 
                   (ULONG)bytesToRead,
                    &bytes);
        *pBytesRead = bytes;
        return;
    }
    // read through all valid pages 
    DbgExt_ReadThrough(offset, 
                       bytesToRead,
                       pBuffer);
    *pBytesRead = bytesToRead;
}


class CExtCaptureOutput:IDebugOutputCallbacks
{
    CExtCaptureOutput(const CExtCaptureOutput&);
    CExtCaptureOutput& operator =(const CExtCaptureOutput&);

    IDebugControl * m_pDebugControl;
    IDebugClient4 * m_pDebugClient;
    IDebugOutputCallbacks * m_OldOutCb;
    std::string m_result;
    bool m_started;
    typedef IDebugOutputCallbacks BaseClass_type;
public:
    CExtCaptureOutput(IDebugControl * pDebugControl,
                      IDebugClient4 * pDebugClient)
        :
            m_OldOutCb(0), 
            m_started(false),
            m_pDebugControl(pDebugControl),
            m_pDebugClient(pDebugClient)
    {
    }
    ~CExtCaptureOutput()
    {
        Stop();
    }

    std::string & GetString()    {        return m_result;    }
    const std::string & GetString() const     {        return m_result;    }
    
    // IUnknown.
    STDMETHOD(QueryInterface)(
        THIS_
        _In_ REFIID InterfaceId,
        _Out_ PVOID* Interface
        )
    {
        *Interface = NULL;

        if (IsEqualIID(InterfaceId, __uuidof(IUnknown)) ||
            IsEqualIID(InterfaceId, __uuidof(BaseClass_type)))
        {
            *Interface = (BaseClass_type *)this;
            AddRef();
            return S_OK;
        }
        else
        {
            return E_NOINTERFACE;
        }
    }
    STDMETHOD_(ULONG, AddRef)(
        THIS
        )
    {
        // This class is designed to be non-dynamic so
        // there's no true refcount.
        return 1;
    }
    STDMETHOD_(ULONG, Release)(
        THIS
        )
    {
        // This class is designed to be non-dynamic so
        // there's no true refcount.
        return 0;
    }
    
    // IDebugOutputCallbacks*.
    STDMETHOD(Output)(
        THIS_
        _In_ ULONG Mask,
        _In_ const char * Text
        )
    {
        m_result.append(Text);
        return S_OK;
    }

    void Start()
    {
        HRESULT status = 0;

        if ((status = m_pDebugClient->GetOutputCallbacks(&m_OldOutCb)) != S_OK)
        {
            throw std::runtime_error("Unable to get previous output callback");
        }
        if ((status = m_pDebugClient->SetOutputCallbacks(this)) != S_OK)
        {
            throw std::runtime_error("Unable to set capture output callback");
        }
        m_started = true;
    }
    
    void Stop()
    {
        if (!m_started)
        {
            return;
        }
        m_pDebugClient->SetOutputCallbacks(m_OldOutCb);
        m_started = false;
        m_OldOutCb = NULL;
    }

    void Delete(void)
    {
        if (m_started)
        {
            Stop();
        }
    }

    void Execute(const char * pCommand)
    {
        Start();
        
        // Hide all output from the execution
        // and don't save the command.
        m_pDebugControl->Execute(DEBUG_OUTCTL_THIS_CLIENT |
                                  DEBUG_OUTCTL_OVERRIDE_MASK |
                                  DEBUG_OUTCTL_NOT_LOGGED,
                                  pCommand,
                                  DEBUG_EXECUTE_NOT_LOGGED |
                                  DEBUG_EXECUTE_NO_REPEAT);

        Stop();
    }
   
};


void ExecuteCustomCommand(IDebugControl * pDebugControl,
                          IDebugClient4 * pDebugClient,
                          const std::string & command, 
                          std::string * pResult)
{
    CExtCaptureOutput captureHelper(pDebugControl, pDebugClient);
    captureHelper.Execute(command.c_str());
    pResult->swap(captureHelper.GetString());
}

static int CharToDigit(char ch)
{
    if (ch >='0' && ch <='9')
        return (int)(ch-'0');
    if (ch >='a' && ch <='f')
        return (int)(ch-'a')+0xa;
    if (ch >='A' && ch <='F')
        return (int)(ch-'A')+0xa;
    return -1;
}
class CHexToBinaryWriter
{
    char * m_pBuffer;
    orthia::Address_type m_bytesToRead;
    orthia::Address_type * m_pBytesRead;
public:
    CHexToBinaryWriter(void * pBuffer,
                       orthia::Address_type bytesToRead,
                       orthia::Address_type * pBytesRead)
        :
            m_pBuffer((char *)pBuffer),
            m_bytesToRead(bytesToRead),
            m_pBytesRead(pBytesRead)
    {
        *m_pBytesRead = 0;
    }
    bool WriteTextLine(const char * pLineStart,
                       const char * pLineEnd)
    {
        if (!m_pBytesRead)
        {
            return true;
        }
        int i = 0;
        for(const char * p = pLineStart; p+1< pLineEnd && i < 0x10; p+=3, ++i)
        {
            int value1 = CharToDigit(p[0]);
            int value2 = CharToDigit(p[1]);
            if (value1 == -1 || value2 == -1)
            {
                return true;
            }
            char result = (char)((value1<<4)|(value2 & 0xF));

            *m_pBuffer = result;
            ++m_pBuffer;
            --m_bytesToRead;
            ++*m_pBytesRead;
            if (!m_bytesToRead)
            {
                return true;
            }
        }
        return false;
    }
};

void Orthia_CustomMemoryReadEx(void * pBuffer,
                               orthia::Address_type bytesToRead,
                               orthia::Address_type * pBytesRead,
                               const std::string & addressText)
{
    *pBytesRead = 0;
    IDebugControl * pDebugControl = ExtQueryControl();
    IDebugClient4 * pDebugClient = ExtQueryClient();

    if (!pDebugControl || !pDebugClient)
    {
        return;
    }

    std::stringstream commandStream;
    std::hex(commandStream);
    commandStream<<"db "<<addressText<<" L0x"<<bytesToRead<<"\n";

    std::string result;
    ExecuteCustomCommand(pDebugControl, 
                         pDebugClient, 
                         commandStream.str(), 
                         &result);

    std::vector<StringInfo_Ansi> parts;
    std::vector<std::string> lines;
    orthia::Split<char>(result, &lines, 0x0A);

    CHexToBinaryWriter writer(pBuffer, bytesToRead, pBytesRead);

     for(std::vector<std::string>::iterator it = lines.begin(), it_end = lines.end();
        it != it_end;
        ++it)
    {
        orthia::SplitString(*it, "  ", &parts);

        if (parts.size() <2)
            continue;

        if (writer.WriteTextLine(parts[1].m_pBegin, parts[1].m_pEnd))
        {
            break;
        }
    }
}

void Orthia_CustomMemoryRead(orthia::Address_type offset,
                             void * pBuffer,
                             orthia::Address_type bytesToRead,
                             orthia::Address_type * pBytesRead,
                             DianaUnifiedRegister selectorHint)
{
    std::stringstream arg;
    std::hex(arg);
    // Note: this looks strange, but "db gs:blabla" works better than "db 2b:blabla" so be it
    switch(selectorHint)
    {
    case reg_ES:
        arg<<"es";
        break;
    case reg_CS:
        arg<<"cs";
        break;
    case reg_SS:
        arg<<"ss";
        break;
    case reg_DS:
        arg<<"ds";
        break;
    case reg_FS:
        arg<<"fs";
        break;
    case reg_GS:
        arg<<"gs";
        break;
    default:
        throw std::runtime_error("Invalid register");
    }
    arg<<":0x"<<offset;
    Orthia_CustomMemoryReadEx(pBuffer,
                              bytesToRead,
                              pBytesRead,
                              arg.str());
}
// COrthiaDebugger
COrthiaDebugger::COrthiaDebugger()
{
}
bool COrthiaDebugger::IsInterrupted()
{
    IDebugControl * pDebugControl = ExtQueryControl();
    if (!pDebugControl)
    {
        return true;
    }
    return pDebugControl->GetInterrupt() == S_OK;
}

//  COrthiaPeLinkImportsObserver
COrthiaPeLinkImportsObserver::COrthiaPeLinkImportsObserver(COrthiaWindbgAPIHandlerDebugInterface * pDebugInterface)
    :
        m_pDebugInterface(pDebugInterface)
{
}
void COrthiaPeLinkImportsObserver::QueryFunctionByOrdinal(const char * pDllName,
                                                          DI_UINT32 ordinal,
                                                          OPERAND_SIZE * pAddress)
{
    *pAddress = m_pDebugInterface->QueryFunctionAddressByOrdinal(pDllName, 
                                                                 ordinal);

}

void COrthiaPeLinkImportsObserver::QueryFunctionByName(const char * pDllName,
                                                       const char * pFunctionName,
                                                       DI_UINT32 hint,
                                                       OPERAND_SIZE * pAddress)
{
    *pAddress = m_pDebugInterface->QueryFunctionAddress(0,
                                                        pDllName, 
                                                        pFunctionName);
}


//  COrthiaWindbgAPIHandlerDebugInterface
COrthiaWindbgAPIHandlerDebugInterface::COrthiaWindbgAPIHandlerDebugInterface()
    :
        m_dianaMode(0),
        m_printHappened(false),
        m_pVirtualEnvironment(0)
{
}
void COrthiaWindbgAPIHandlerDebugInterface::RegisterModule(const std::string & windbgName,
                                                           const std::string & fullName, 
                                                           OPERAND_SIZE moduleStart, 
                                                           OPERAND_SIZE moduleEnd)
{
    std::string fileName;
    UnparseFileNameFromFullFileName(fullName, &fileName);
    fileName = NormalizeModuleName(fileName);

    std::pair<ModulesMapByName_type::iterator, bool> 
        res = m_modulesMapByName.insert(std::make_pair(fileName, moduleStart));
    if (!res.second)
    {
        return;
    }
    m_modulesMapByOffset.insert(std::make_pair(moduleStart, ModuleInfo(windbgName, fullName, moduleStart, moduleEnd)));
}
std::string COrthiaWindbgAPIHandlerDebugInterface::NormalizeModuleName(const std::string & fullName)
{
    return orthia::Downcase_Ansi(fullName);
}
void COrthiaWindbgAPIHandlerDebugInterface::Init(CMemoryStorageOfModifiedData * pVirtualEnvironment)
{
    m_pVirtualEnvironment = pVirtualEnvironment;
}
void COrthiaWindbgAPIHandlerDebugInterface::Reload(int dianaMode)
{
/*0:000> lmpf
start    end        module name
002f0000 00414000   orthia_test W:\di_trunk\trunk\bin\Release\i386\orthia_test.exe
75170000 752b0000   KERNEL32 C:\WINDOWS\SYSTEM32\KERNEL32.DLL
759b0000 75a87000   KERNELBASE C:\WINDOWS\SYSTEM32\KERNELBASE.dll
75a90000 75a96000   PSAPI    C:\WINDOWS\SYSTEM32\PSAPI.DLL
776a0000 7780f000   ntdll    C:\WINDOWS\SYSTEM32\ntdll.dll
*/
    m_dianaMode = dianaMode;
    IDebugControl * pDebugControl = ExtQueryControl();
    IDebugClient4 * pDebugClient = ExtQueryClient();
    if (!pDebugControl || !pDebugClient)
    {
        return;
    }

    std::string result;
    ExecuteCustomCommand(pDebugControl, 
                         pDebugClient, 
                         "lmpf", 
                         &result);

    std::vector<std::string> lines;
    orthia::Split<char>(result, &lines, 0x0A);

    bool skipLine = true;
    DI_UINT64 moduleStart = 0, moduleEnd = 0;
    std::string moduleName;
    std::string moduleFullFileName;
    for(std::vector<std::string>::iterator it = lines.begin(), it_end = lines.end();
        it != it_end;
        ++it)
    {
        if (skipLine)
        {
            skipLine = false;
            continue;
        }
        if (*it == "Unloaded modules:")
        {
            break;
        }
        CWindbgTextIterator<char> iterator(orthia::Trim(*it));
        if (!iterator.ParseToken(&moduleStart, dianaMode, iterator.whitespace()))
        {
            break;
        }
        if (!iterator.ParseToken(&moduleEnd, dianaMode, iterator.whitespace()))
        {
            continue;
        }
        if (!iterator.ParseToken(&moduleName, iterator.whitespace()))
        {
            continue;
        }
        if (!iterator.ParseToken(&moduleFullFileName, iterator.end_of_document()))
        {
            continue;
        }
        // collected all the parts
        RegisterModule(moduleName, moduleFullFileName, moduleStart, moduleEnd);
    }
}
OPERAND_SIZE COrthiaWindbgAPIHandlerDebugInterface::QueryModule(const char * pDllName)
{
    std::string name = NormalizeModuleName(pDllName);
    ModulesMapByName_type::const_iterator it = m_modulesMapByName.find(name);
    if (it == m_modulesMapByName.end())
    {
        return 0;
    }
    return it->second;
}
static IAPIHandlerDebugInterface::Debuggee_type CommonGetDebuggeeType()
{
    IDebugControl * pDebugControl = ExtQueryControl();
    IDebugClient4 * pDebugClient = ExtQueryClient();
    if (!pDebugControl || !pDebugClient)
    {
        return IAPIHandlerDebugInterface::dtNone;
    }
    ULONG classValue = 0;
    ULONG qualifierValue = 0;
    if (!SUCCEEDED(pDebugControl->GetDebuggeeType(&classValue, &qualifierValue)))
    {
        return IAPIHandlerDebugInterface::dtNone;
    }
        
    switch(classValue)
    {
    case DEBUG_CLASS_KERNEL:
        return IAPIHandlerDebugInterface::dtKernel;
    case DEBUG_CLASS_USER_WINDOWS:
        return IAPIHandlerDebugInterface::dtUser;
    }
    return IAPIHandlerDebugInterface::dtNone;
}
IAPIHandlerDebugInterface::Debuggee_type COrthiaWindbgAPIHandlerDebugInterface::GetDebuggeeType()
{
    return CommonGetDebuggeeType();
}
void COrthiaWindbgAPIHandlerDebugInterface::Print(const std::wstring & text)
{
    dprintf("%S", text.c_str());
    m_printHappened = true;
}
bool COrthiaWindbgAPIHandlerDebugInterface::HaveSomePrintsHappened() const
{
    return m_printHappened;
}
OPERAND_SIZE COrthiaWindbgAPIHandlerDebugInterface::QueryFunctionAddressByOrdinal(const char * pDllName,
                                                                                  DI_UINT32 ordinal)
{
    OPERAND_SIZE module = QueryModule(pDllName);
    if (!module)
    {
        return 0;
    }
    ModulesMapByOffset_type::iterator it = m_modulesMapByOffset.find(module);
    if (it == m_modulesMapByOffset.end())
    {
        return 0;
    }
    return QueryFunctionNameEx(it->second, pDllName, 0, ordinal);
}
OPERAND_SIZE COrthiaWindbgAPIHandlerDebugInterface::QueryFunctionNameEx(ModuleInfo & info,
                                                                        const char * pDllName,
                                                                        const char * pFunctionName,
                                                                        DI_UINT32 ordinal)
{
    ModuleInfo * pInfo = &info;
    std::string currentDll = pDllName;
    std::string currentFunction = pFunctionName;
    std::string newDll;
    std::string newFunction;
    for(;;)
    {
        OPERAND_SIZE res = 
            QueryFunctionNameEx(*pInfo,
                                 currentDll.c_str(),
                                 currentFunction.c_str(),
                                 ordinal,
                                 &newDll,
                                 &newFunction);
        if (res)
        {
            return res;
        }
        if (newDll.empty())
        {
            return 0;
        }

        // new ====> current
        currentDll.swap(newDll);
        currentFunction.swap(newFunction);

        currentDll = NormalizeModuleName(currentDll);
        OPERAND_SIZE module = QueryModule(currentDll.c_str());
        if (!module)
        {
            return 0;
        }
        ModulesMapByOffset_type::iterator it = m_modulesMapByOffset.find(module);
        if (it == m_modulesMapByOffset.end())
        {
            return 0;
        }
        pInfo = &it->second;
        ordinal = 0;
    }
}
OPERAND_SIZE COrthiaWindbgAPIHandlerDebugInterface::QueryFunctionNameEx(ModuleInfo & info,
                                                                        const char * pDllName,
                                                                        const char * pFunctionName,
                                                                        DI_UINT32 ordinal,
                                                                        std::string * pTargetDll,
                                                                        std::string * pTargetFunction)
{
    pTargetDll->clear();
    pTargetFunction->clear();
    if (!m_pVirtualEnvironment)
    {
        return 0;
    }
    if (info.preloadedModule.empty())
    {
        VmMemoryRangesTargetOverVectorPlain module;
        m_pVirtualEnvironment->ReportRegions(info.moduleStart,
                                            info.moduleEnd - info.moduleStart,
                                            &module,
                                            true);
        info.preloadedModule.swap(module.m_data);
    }
    if (info.preloadedModule.empty())
    {
        return 0;
    }
    if (!info.pDianaContext)
    {
        info.pDianaContext = new DianaSharedContext();

        OPERAND_SIZE offset = 0;

        
        DianaMovableReadStreamOverMemory_Init(&info.pDianaContext->peFileStream, 
                                               &info.preloadedModule.front(), 
                                               info.preloadedModule.size());
        DI_CHECK_CPP(DianaPeFile_Init(&info.pDianaContext->peFile,
                                      &info.pDianaContext->peFileStream.stream,
                                      info.preloadedModule.size(),
                                      DIANA_PE_FILE_FLAGS_MODULE_MODE));

        info.pDianaContext->peFileGuard.reset(&info.pDianaContext->peFile);

    }
    if (ordinal)
    {
        return 0;
    }
    if (pFunctionName)
    {
        OPERAND_SIZE offset = 0;
        OPERAND_SIZE forwardOffset = 0;
        DI_CHECK_CPP(DianaPeFile_GetProcAddress(&info.pDianaContext->peFile,
                                                &info.preloadedModule.front(),
                                                &info.preloadedModule.front() + info.preloadedModule.size(),
                                                pFunctionName,
                                                &offset,
                                                &forwardOffset));
        if (offset)
        {
            return offset + info.moduleStart;
        }
        std::string forwardInfo(&info.preloadedModule.front() + forwardOffset);
        std::vector<orthia::StringInfo_Ansi> parts;
        orthia::SplitString(forwardInfo, ".", &parts);
        if (parts.size() != 2)
        {
            return 0;
        }
        *pTargetDll = parts[0].ToString() + ".dll";
        *pTargetFunction = parts[1].ToString();
    }
    return 0;
   
}
OPERAND_SIZE COrthiaWindbgAPIHandlerDebugInterface::QueryFunctionAddress(OPERAND_SIZE module,
                                                                         const char * pDllName, 
                                                                         const char * pFunctionName)
{
    if (!module)
    {
        if (!pDllName)
        {
            return 0;
        }
        module = QueryModule(pDllName);
        if (!module)
        {
            return 0;
        }
    }
    ModulesMapByOffset_type::iterator it = m_modulesMapByOffset.find(module);
    if (it == m_modulesMapByOffset.end())
    {
        return 0;
    }
    IDebugControl * pDebugControl = ExtQueryControl();
    IDebugClient4 * pDebugClient = ExtQueryClient();
    if (!pDebugControl || !pDebugClient || !pFunctionName)
    {
        return 0;
    }
    std::string args;
    args.append("x ");
    args.append(it->second.windbgName);
    args.append("!");
    args.append(pFunctionName);
    std::string result;
    ExecuteCustomCommand(pDebugControl, 
                         pDebugClient, 
                         args, 
                         &result);

    std::vector<std::string> lines;
    orthia::Split<char>(result, &lines, 0x0A);

//    0: kd> x nt!PsGetCurrentPRocess
//fffff800`9ef4d510 nt!PsGetCurrentProcess (<no parameter info>)
    if (lines.empty())
    {
        return QueryFunctionNameEx(it->second, pDllName, pFunctionName, 0);
    }

    try
    {
        CWindbgTextIterator<char> iterator(orthia::Trim(lines[lines.size()-1]));

        OPERAND_SIZE result = 0;
        if (iterator.ParseToken(&result, m_dianaMode, iterator.whitespace()))
        {
            return result;
        }
    }
    catch(std::exception & ex)
    {
        &ex;
    }
    return 0;
}
COrthiaWindbgAPIHandlerDebugInterface::ModuleInfo* COrthiaWindbgAPIHandlerDebugInterface::QueryModuleInfo(OPERAND_SIZE address)
{
    ModulesMapByOffset_type::iterator it = m_modulesMapByOffset.find(address);
    if (it == m_modulesMapByOffset.end())
    {
        return 0;
    }
    return &it->second;
}

CWindbgAddressSpace::CWindbgAddressSpace()
{
    switch(QueryInitialDianaMode())
    {
    case DIANA_MODE32:
        m_maxValid = 0x7FFFFFFF;
        m_maxValidKernel = 0xFFFFFFFF;
        break;
    case DIANA_MODE64:
        m_maxValid = 0x7FFFFFFFFFFFFFFFULL;
        m_maxValidKernel = 0xFFFFFFFFFFFFFFFFULL;
        break;
    default:
        throw std::runtime_error("Unsupported system");
    }
}
bool CWindbgAddressSpace::IsRegionFree(Address_type offset, 
                                       Address_type size,
                                       Address_type * startOfNewRegion)
{
    return DbgExt_IsRegionFree(offset, size, startOfNewRegion);
}
OPERAND_SIZE CWindbgAddressSpace::GetMaxValidPointer() const
{
    return m_maxValid;
}

}