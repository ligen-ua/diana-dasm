#include "orthia_windbg_utils.h"
#include "orthia.h"
#include "orthia_module_manager.h"
#include "orthia_memory_cache.h"


namespace orthia
{

std::string QueryRegName(const std::string & base)
{
    bool wow64 = false;
    ULONG machine = DbgExt_GetCurrentModeOfTargetMachine(&wow64);
    int mode = 0;
    switch(machine)
    {
    case IMAGE_FILE_MACHINE_I386:
        return "e" + base;
    case IMAGE_FILE_MACHINE_AMD64:
        return "r" + base;
    }
    throw std::runtime_error("Unknown archirecture");
}
 
orthia::Address_type QueryRegValue(const std::string & base)
{
    orthia::Address_type regValue = 0;
    std::string fullRegName = QueryRegName(base);
    PCSTR tail = 0;
    if (!GetExpressionEx(fullRegName.c_str(), &regValue, &tail))
    {
        throw std::runtime_error("Can't access: " + fullRegName);
    }
    return regValue;
}


void PrintResult(Diana_Processor_Registers_Context * pContext, 
                 int emulationResult,
                 int dianaMode)
{
    std::string errorCode = diana::QueryErrorCode(emulationResult);
    dprintf("Diana Error Code: %s\n", errorCode.c_str());

    if (dianaMode == DIANA_MODE32)
    {
        dprintf("eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n"
                "eip=%08x esp=%08x ebp=%08x\n"
                "cs=%04x  ss=%04x  ds=%04x  es=%04x  fs=%04x gs=%04x efl=%08x\n"
        ,
            (DI_UINT32)pContext->reg_RAX.value,
            (DI_UINT32)pContext->reg_RBX.value,
            (DI_UINT32)pContext->reg_RCX.value,
            (DI_UINT32)pContext->reg_RDX.value,
            (DI_UINT32)pContext->reg_RSI.value,
            (DI_UINT32)pContext->reg_RDI.value,
            (DI_UINT32)pContext->reg_RIP.value,
            (DI_UINT32)pContext->reg_RSP.value,
            (DI_UINT32)pContext->reg_RBP.value,
            (DI_UINT32)pContext->reg_CS.value,
            (DI_UINT32)pContext->reg_SS.value,
            (DI_UINT32)pContext->reg_DS.value,
            (DI_UINT32)pContext->reg_ES.value,
            (DI_UINT32)pContext->reg_FS.value,
            (DI_UINT32)pContext->reg_GS.value,
            (DI_UINT32)pContext->flags.value);
        return;
    }
    if (dianaMode == DIANA_MODE64)
    {
        dprintf("rax=%p rbx=%p rcx=%p\n"
                "rdx=%p rsi=%p rdi=%p\n"
                "rip=%p rsp=%p rbp=%p\n"
                " r8=%p  r9=%p r10=%p\n"
                "r11=%p r12=%p r13=%p\n"
                "r14=%p r15=%p\n"
                "cs=%04x  ss=%04x  ds=%04x  es=%04x  fs=%04x  gs=%04x  efl=%08x\n"
        ,
            pContext->reg_RAX.value,
            pContext->reg_RBX.value,
            pContext->reg_RCX.value,
            pContext->reg_RDX.value,
            pContext->reg_RSI.value,
            pContext->reg_RDI.value,
            pContext->reg_RIP.value,
            pContext->reg_RSP.value,
            pContext->reg_RBP.value,
            pContext->reg_R8.value,
            pContext->reg_R9.value,
            pContext->reg_R10.value,
            pContext->reg_R11.value,
            pContext->reg_R12.value,
            pContext->reg_R13.value,
            pContext->reg_R14.value,
            pContext->reg_R15.value,
            (DI_UINT32)pContext->reg_CS.value,
            (DI_UINT32)pContext->reg_SS.value,
            (DI_UINT32)pContext->reg_DS.value,
            (DI_UINT32)pContext->reg_ES.value,
            (DI_UINT32)pContext->reg_FS.value,
            (DI_UINT32)pContext->reg_GS.value,
            (DI_UINT32)pContext->flags.value);
        return;
    }
    throw std::runtime_error("Incorrect argument");
}

static 
char EscapeSymbol(char symbol)
{
    return orthia::AsciiEscapeSymbol(symbol);
}

void PrintPage(orthia::Address_type addressOfPage,
               const std::vector<char> & data,
               int dianaMode)
{
    if (data.empty() || data.size() % 0x10)
    {
        throw std::runtime_error("Invalid page");
    }
    const char * pStart = &data.front();
    const char * pEnd = pStart + data.size();

    orthia::Address_type currentAddress = addressOfPage;
    for(; pStart != pEnd; pStart += 0x10, currentAddress += 0x10)
    {
        dprintf("%p  %.2hhx %.2hhx %.2hhx %.2hhx %.2hhx %.2hhx %.2hhx %.2hhx-%.2hhx %.2hhx %.2hhx %.2hhx %.2hhx %.2hhx %.2hhx %.2hhx  ",
                 currentAddress,
                 (0xff&(int)pStart[0]),
                 (0xff&(int)pStart[1]),
                 (0xff&(int)pStart[2]),
                 (0xff&(int)pStart[3]),
                 (0xff&(int)pStart[4]),
                 (0xff&(int)pStart[5]),
                 (0xff&(int)pStart[6]),
                 (0xff&(int)pStart[7]),
                 (0xff&(int)pStart[8]),
                 (0xff&(int)pStart[9]),
                 (0xff&(int)pStart[10]),
                 (0xff&(int)pStart[11]),
                 (0xff&(int)pStart[12]),
                 (0xff&(int)pStart[13]),
                 (0xff&(int)pStart[14]),
                 (0xff&(int)pStart[15]));

        dprintf("%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
                 EscapeSymbol(pStart[0]),
                 EscapeSymbol(pStart[1]),
                 EscapeSymbol(pStart[2]),
                 EscapeSymbol(pStart[3]),
                 EscapeSymbol(pStart[4]),
                 EscapeSymbol(pStart[5]),
                 EscapeSymbol(pStart[6]),
                 EscapeSymbol(pStart[7]),
                 EscapeSymbol(pStart[8]),
                 EscapeSymbol(pStart[9]),
                 EscapeSymbol(pStart[10]),
                 EscapeSymbol(pStart[11]),
                 EscapeSymbol(pStart[12]),
                 EscapeSymbol(pStart[13]),
                 EscapeSymbol(pStart[14]),
                 EscapeSymbol(pStart[15]));
    }

}

void PrintData(orthia::CMemoryStorageOfModifiedData & allWrites,
               int dianaMode)
{
    dprintf("Modified pages:\n");
    for (orthia::CMemoryStorageOfModifiedData::const_iterator it = allWrites.begin(), it_end = allWrites.end();
        it != it_end;
        ++it)
    {
        PrintPage(it->first, it->second.data, dianaMode);
    }
    dprintf("Done\n");
}


std::wstring UnescapeArg(const std::wstring & value)
{
    if (value.size() > 2 && value[0] == L'\"' && value[value.size()-1] == L'\"')
    {
        return std::wstring(value.c_str()+1, value.c_str()+value.size()-2);
    }
    return value;
}
static void CheckExactMatch(PCSTR tail)
{
    for (; *tail; ++tail)
    {    
        if (*tail != ' ')     
            throw std::runtime_error("Invalid expression");
        
    }
}
template<class Deserializer_type>
const char * CommonReadExpressitonValue(const char * args, 
                                       Deserializer_type & deserializer, 
                                       bool exactMatch)
{
    const char * pTail = args;
    for(;;++pTail)
    {
        if (*pTail == 0)
        {
            throw std::runtime_error("Invalid argument");
        }
        if (*pTail != ' ')
            break;
    }
    const char * pStr = pTail;
    for(++pTail;*pTail;++pTail)
    {
        if (*pTail == ' ')
        {
            if (exactMatch)
            {
                CheckExactMatch(pTail);
            }
            break;
        }
    }
    std::string value(pStr, pTail);
    if (deserializer.TryDeserialize(value))
    {
        return pTail;
    }
    Address_type value64 = 0;
    const char * fakeTail = 0;
    if (!GetExpressionEx(value.c_str(), &value64, &fakeTail))
    {
        throw std::runtime_error("Can't unparse command line argument");
    }
    deserializer.Init(value64);
    return pTail;
}
struct DeserializerLongLong
{
    long long * m_pResult;
    DeserializerLongLong(long long * pResult)
        :
            m_pResult(pResult)
    {
    }
    bool TryDeserialize(const std::string & value)
    {
        PCSTR tail = 0;
        char * pLocalEnd = (char *)value.c_str();
        *m_pResult = _strtoi64(value.c_str(), &pLocalEnd, 16);
        return !(pLocalEnd == value.c_str() || (*pLocalEnd != ' ' && *pLocalEnd != 0));
    }   
    void Init(Address_type op)
    {
        *m_pResult = (long long)op;
    }
};
const char * ReadExpressitonValue(const char * args, 
                                  long long & value, 
                                  bool exactMatch)
{
    DeserializerLongLong deserializer(&value);
    return CommonReadExpressitonValue(args, 
                                      deserializer, 
                                      exactMatch);
}

struct DeserializerAddressType
{
    Address_type * m_pResult;
    DeserializerAddressType(Address_type * pResult)
        :
            m_pResult(pResult)
    {
    }
    bool TryDeserialize(const std::string & value)
    {
        PCSTR tail = 0;
        char * pLocalEnd = (char *)value.c_str();
        *m_pResult = _strtoui64(value.c_str(), &pLocalEnd, 16);
        return !(pLocalEnd == value.c_str() || (*pLocalEnd != ' ' && *pLocalEnd != 0));
    }   
    void Init(Address_type op)
    {
        *m_pResult = op;
    }
};
const char * ReadExpressitonValue(const char * args, 
                                  orthia::Address_type & value, 
                                  bool exactMatch)
{
    DeserializerAddressType deserializer(&value);
    return CommonReadExpressitonValue(args, 
                                      deserializer, 
                                      exactMatch);
}

struct DeserializerStringType
{
    std::string * m_pResult;
    DeserializerStringType(std::string * pResult)
        :
            m_pResult(pResult)
    {
    }
    bool TryDeserialize(const std::string & value)
    {
        *m_pResult = value;
        return true;
    }   
    void Init(Address_type op)
    {
        throw std::runtime_error("Internal error");
    }
};
const char * ReadExpressitonValue(const char * args, 
                                  std::string & value, 
                                  bool exactMatch)
{
    DeserializerStringType deserializer(&value);
    return CommonReadExpressitonValue(args, 
                                      deserializer, 
                                      exactMatch);
}
void CPrintfWriter::PrintLine(const std::wstring & line)
{
    dprintf("%S\n",  line.c_str());
}

const char * ReadWindbgSize(const char * pTail, 
                            orthia::Address_type * pSize,
                            bool exactMatch,
                            bool checkPrefixAndSilentlyFail)
{
    for(;;++pTail)
    {
        if (*pTail == 0)
        {
            throw std::runtime_error("Invalid argument: range");
        }
        if (*pTail != ' ')
            break;
    }
    const char * pStr = pTail;
    if (*pStr != L'L')
    {
        if (checkPrefixAndSilentlyFail)
        {
            return pStr;
        }
        throw std::runtime_error("Invalid argument: range");
    }
    for(++pTail;*pTail;++pTail)
    {
        if (*pTail == ' ')
        {
            if (exactMatch)
            {
                CheckExactMatch(pTail);
            }
            break;
        }
    }

    int toSkip = 1;
    if ((pTail - pStr) > 2 && pStr[1] == '0')
    {
        if (pStr[2] == 'x')
        {
            toSkip = 3;
        }
        else
        if (pStr[2] == 'n')
        {
            orthia::StringToObject(std::wstring(pStr+3, pTail), pSize);
            return pTail;
        }
    }
    orthia::HexStringToObject(std::wstring(pStr+toSkip, pTail), pSize);
    return pTail;
}

}