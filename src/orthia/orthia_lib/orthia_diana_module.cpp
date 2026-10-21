#include "orthia_diana_module.h"
#include "orthia_memory_cache.h"
#include <iomanip>
#include <algorithm>
#include "orthia_sha1.h"

extern "C"
{
#include "diana_pe_analyzer.h"
#include "diana_elf_analyzer.h"
}
#include "diana_core_cpp.h"
#include "orthia_streams.h"

namespace orthia
{

static
int DianaEnvironment_ConvertAddressToRelative(void * pThis, 
                                              OPERAND_SIZE address,              
                                              OPERAND_SIZE * pRelativeOffset,
                                              int * pbInvalidPointer);

static
int DianaEnvironment_AnalyzeJumpAddress(void * pThis, 
                                       OPERAND_SIZE address,
                                       int flags,
                                       OPERAND_SIZE * pRelativeOffset,
                                       DianaAnalyzeAddressResult_type * pResult);


struct DianaEnvironment:public DianaAnalyzeObserver
{
    Address_type m_moduleStart;
    Address_type m_moduleSize;
    int m_dianaMode;
    DianaMemoryStream m_stream;

    DianaEnvironment(Address_type moduleStart, IMemoryReader * pMemoryReader)
        :
            m_stream(0, pMemoryReader, 0),
            m_moduleStart(moduleStart),
            m_moduleSize(0),
            m_dianaMode(0)
    {
        DianaAnalyzeObserver_Init(this, 
                                   &m_stream.parent, 
                                  DianaEnvironment_AnalyzeJumpAddress); 
    }
};

static 
int DianaEnvironment_ConvertAddressToRelative(void * pThis, 
                                              OPERAND_SIZE address,               
                                              OPERAND_SIZE * pRelativeOffset,
                                              int * pbInvalidPointer)
{
    DianaEnvironment * pEnv = (DianaEnvironment * )pThis;
    *pbInvalidPointer = 1;
    
    // windbg-plugin-related behavior
    if ((long long)pEnv->m_moduleStart < 0 && pEnv->m_dianaMode == DIANA_MODE32)
    {
        if ((long long)address > 0 && address & 0x80000000)
        {
            // sign extend the address
            orthia::UnsignedLargeInteger_type temp;
            temp.QuadPart = address;
            temp.HighPart = (uint32_t)DI_MAX_OPERAND_SIZE;
            address = temp.QuadPart;
            *pRelativeOffset = address; 
        }
    }

    if (address < pEnv->m_moduleStart)
        return DI_SUCCESS;

    if (address - pEnv->m_moduleStart >= pEnv->m_moduleSize)
        return DI_SUCCESS;

    *pbInvalidPointer = 0;
    *pRelativeOffset = address - pEnv->m_moduleStart; 
    return DI_SUCCESS;
}

static 
int DianaEnvironment_AnalyzeJumpAddress(void * pThis, 
                                       OPERAND_SIZE address,
                                       int flags,
                                       OPERAND_SIZE * pRelativeOffset,
                                       DianaAnalyzeAddressResult_type * pResult)
{
    DianaEnvironment * pEnv = (DianaEnvironment * )pThis;
    *pResult = diaJumpNormal;
    *pRelativeOffset = address;

    if (!pEnv->m_moduleSize)
    {
        Diana_FatalBreak();
    }
    if (flags&DIANA_ANALYZE_ABSOLUTE_ADDRESS)
    {
        int invalidPointer = 0;
        DI_CHECK(DianaEnvironment_ConvertAddressToRelative(pThis, 
                                                           address, 
                                                           pRelativeOffset,
                                                           &invalidPointer));
        if (invalidPointer)
        {
            *pResult = diaJumpExternal;
            return DI_SUCCESS;
        }
        address = *pRelativeOffset;
    }
    if (address >= pEnv->m_moduleSize)
    {
        *pRelativeOffset = address + pEnv->m_moduleStart;
        *pResult = diaJumpExternal;
    }
    return DI_SUCCESS;
}

// the route stack of one analysis
class CAnalyzeStack
{
    Diana_Stack m_stack;
    CAnalyzeStack(const CAnalyzeStack&);
    CAnalyzeStack& operator = (const CAnalyzeStack&);
public:
    CAnalyzeStack()
    {
        DI_CHECK_CPP(Diana_Stack_Init(&m_stack, 4096, sizeof(Diana_RouteInfo)));
    }
    ~CAnalyzeStack()
    {
        Diana_Stack_Free(&m_stack);
    }
    Diana_Stack* Get() { return &m_stack; }
};

class CDianaModuleImpl
{
protected:
    friend class CDianaInstructionIterator;
    friend class CDianaModule;
    CMemoryCache m_cache;
    DianaEnvironment m_env;
    Diana_InstructionsOwner m_owner;
    diana::Guard<diana::InstructionsOwner> m_instructionsOwnerGuard;
    void InitEnv(OPERAND_SIZE size, int mode)
    {
        m_env.m_dianaMode = mode;
        m_env.m_moduleSize = size;
        m_env.m_stream.m_moduleSize = size;
    }
    virtual void AnalyzeImpl(int analyserFlags)=0;
    virtual void AnalyzeWithHintsImpl(int analyserFlags,
                                      const std::vector<Address_type>& extraEntryPoints) = 0;
public:
    CDianaModuleImpl(Address_type offset,
                     IMemoryReader * pMemoryReader)
    :
        m_cache(pMemoryReader, offset),
        m_env(offset, &m_cache)
    {
    }
    virtual ~CDianaModuleImpl()
    {
    }
    void Analyze(int analyserFlags)
    {
        AnalyzeImpl(analyserFlags);
    }
    void AnalyzeWithHints(int analyserFlags,
                          const std::vector<Address_type>& extraEntryPoints)
    {
        AnalyzeWithHintsImpl(analyserFlags, extraEntryPoints);
    }
    orthia::Address_type GetModuleSize() { return m_env.m_moduleSize; }
    virtual int GetDianaMode() const =0;
    virtual int GetDianaArch() const =0;
};

class CDianaModuleImpl_Executable:public CDianaModuleImpl
{
    DianaExecutable m_executable;
    diana::Guard<diana::ExecutableFile> m_executableGuard;
protected:
    void AnalyzeImpl(int analyserFlags)
    {
        if (m_executable.type == DIANA_EXECUTABLE_TYPE_PE)
        {
            m_cache.Init(m_env.m_moduleStart,
                         m_env.m_moduleSize,
                         m_executable.u.peFile.pImpl->pCapturedSections,
                         m_executable.u.peFile.pImpl->capturedSectionCount);
            DI_CHECK_CPP(Diana_PE_AnalyzePE(&m_executable.u.peFile,
                                             &m_env,
                                             &m_owner,
                                             analyserFlags));
        }
        else  // ELF
        {
            m_cache.Init(m_env.m_moduleStart, m_env.m_moduleSize);
            DI_CHECK_CPP(Diana_ELF_AnalyzeELF(&m_executable.u.elfFile,
                                               &m_env,
                                               &m_owner,
                                               analyserFlags));
            // AArch32: the analysis found the module's own mode
            m_executable.arch = m_executable.u.elfFile.arch;
        }
        m_instructionsOwnerGuard.reset(&m_owner);
    }
    void AnalyzeWithHintsImpl(int analyserFlags,
                              const std::vector<Address_type>& extraEntryPoints) override
    {
        AnalyzeImpl(analyserFlags);
        // the same parameters the PE and ELF analyzers use
        DianaAnalyzeParams params;
        DianaAnalyzeParams_Init(&params, m_executable.arch);
        if (m_executable.arch != DIANA_ARCH_X86)
        {
            params.flags = DIANA_ANALYZE_ADDRESSES_RELATIVE;
        }
        CAnalyzeStack stack;
        for (auto addr : extraEntryPoints)
        {
            if (addr < m_env.m_moduleStart)
                continue;
            Address_type relOffset = addr - m_env.m_moduleStart;
            if (relOffset >= m_env.m_moduleSize)
                continue;
            Diana_AnalyzeCodeEx2(&m_owner, &m_env, GetDianaMode(), relOffset, m_env.m_moduleSize,
                                 stack.Get(), 0, &params);
        }
    }
public:
    CDianaModuleImpl_Executable(Address_type offset,
                                IMemoryReader * pMemoryReader)
        :
            CDianaModuleImpl(offset, pMemoryReader)
    {
        DI_CHECK_CPP(DianaExecutable_Init(&m_executable,
                                          &m_env.m_stream.parent,
                                          0,
                                          DIANA_EXECUTABLE_FILE_FLAGS_MODULE_MODE));
        m_executableGuard.reset(&m_executable);
        InitEnv(m_executable.sizeOfModule, m_executable.dianaMode);
    }
    ~CDianaModuleImpl_Executable()
    {
    }
    int GetDianaMode() const { return m_executable.dianaMode; }
    int GetDianaArch() const { return m_executable.arch; }
};

class CDianaModuleImpl_Range:public CDianaModuleImpl
{
    int m_dianaMode;
    int m_dianaArch;
protected:
    // from the start of the range; DI_ANALYSE_PE_FILE_SCAN_THROUGH: also every place nothing refers to
    void AnalyzeImpl(int analyserFlags)
    {
        m_cache.Init(m_env.m_moduleStart, 
                     m_env.m_moduleSize); 
        DI_CHECK_CPP(Diana_InstructionsOwner_Init(&m_owner, 
                                              m_env.m_moduleSize, 
                                              0x10000));
        m_instructionsOwnerGuard.reset(&m_owner);

        DianaAnalyzeParams params;
        DianaAnalyzeParams_Init(&params, m_dianaArch);
        // the range is where the user put it: ADRP pages and instruction alignment count from there
        params.imageBase = m_env.m_moduleStart;
        CAnalyzeStack stack;
        const OPERAND_SIZE size = m_env.m_moduleSize;
        DI_CHECK_CPP(Diana_AnalyzeCodeEx2(&m_owner, &m_env, m_dianaMode, 0, size, stack.Get(), 0, &params));
        DI_CHECK_CPP(Diana_AnalyzeLandingPads(&m_owner, &m_env, m_dianaMode, 0, size, size, stack.Get(), &params));
        if (analyserFlags & DI_ANALYSE_PE_FILE_SCAN_THROUGH)
        {
            DI_CHECK_CPP(Diana_AnalyzeScanThrough(&m_owner, &m_env, m_dianaMode, 0, size, size, stack.Get(), &params));
        }
    }
    void AnalyzeWithHintsImpl(int analyserFlags,
                              const std::vector<Address_type>&) override
    {
        AnalyzeImpl(analyserFlags);
    }
public:
    CDianaModuleImpl_Range(Address_type offset,
                           Address_type size,
                           IMemoryReader * pMemoryReader,
                           int dianaMode,
                           int dianaArch)
           :
            CDianaModuleImpl(offset, pMemoryReader),
            m_dianaMode(dianaMode),
            m_dianaArch(dianaArch)
    {
        InitEnv(size, dianaMode);
    }
    int GetDianaMode() const { return m_dianaMode; }
    int GetDianaArch() const { return m_dianaArch; }
};

CDianaModule::CDianaModule()
    :
        m_pMemoryReader(0),
        m_offset(0)
{
}

CDianaModule::~CDianaModule()
{
}

Address_type CDianaModule::GetModuleAddress() const
{
    return m_offset;
}
Address_type CDianaModule::GetModuleSize() const
{
    return m_impl->GetModuleSize();
}
int CDianaModule::GetDianaArch() const
{
    return m_impl->GetDianaArch();
}
void CDianaModule::QueryArmCodeMap(std::vector<ArmCodeChange>& changes) const
{
    changes.clear();
    const int arch = m_impl->GetDianaArch();
    if (!IsDianaArchArm32(arch))
    {
        return;
    }
    // the code (T32 or A32 by its flag) and the data the analyzer marked, by offset
    const Diana_InstructionsOwner& owner = m_impl->m_owner;
    std::vector<ArmCodeChange> entries;
    for (OPERAND_SIZE i = 0; i < owner.m_usedSize; ++i)
    {
        const Diana_Instruction& instruction = owner.m_pInstructionsVec[i];
        if (instruction.m_flags & DI_INSTRUCTION_EXTERNAL)
        {
            continue;
        }
        if (instruction.m_flags & DI_INSTRUCTION_INVALID)
        {
            if (instruction.m_flags & DI_INSTRUCTION_DATA)
            {
                const ArmCodeKind kind = (instruction.m_flags & DI_INSTRUCTION_DATA_BYTES) ? ArmCodeKind::Data8 :
                    (instruction.m_flags & DI_INSTRUCTION_DATA_HALFWORDS) ? ArmCodeKind::Data16 : ArmCodeKind::Data;
                entries.push_back({ instruction.m_offset, kind });
            }
            continue;
        }
        entries.push_back({ instruction.m_offset,
            (instruction.m_flags & DI_INSTRUCTION_THUMB) ? ArmCodeKind::T32 : ArmCodeKind::A32 });
    }
    std::sort(entries.begin(), entries.end(), [](const ArmCodeChange& a, const ArmCodeChange& b) {
        return a.offset < b.offset;
    });
    const ArmCodeKind moduleKind = GetArmCodeKind(arch);
    ArmCodeKind current = moduleKind;
    for (size_t k = 0; k < entries.size(); ++k)
    {
        if (entries[k].kind != current)
        {
            changes.push_back(entries[k]);
            current = entries[k].kind;
        }
        // data is marked every 2 bytes: it ends where the marks do
        const DI_UINT64 end = entries[k].offset + 2;
        if (IsArmCodeKindData(current) && (k + 1 == entries.size() || entries[k + 1].offset > end))
        {
            changes.push_back({ end, moduleKind });
            current = moduleKind;
        }
    }
}
void CDianaModule::InitRaw(Address_type offset,
                           Address_type size,
                           IMemoryReader * pMemoryReader,
                           int mode,
                           int arch)
{
    m_offset = offset;
    m_pMemoryReader = pMemoryReader;
    m_impl.reset(new CDianaModuleImpl_Range(offset, size, pMemoryReader, mode, arch));
}
void CDianaModule::Init(Address_type offset,
                        IMemoryReader * pMemoryReader)
{
    m_offset = offset;
    m_pMemoryReader = pMemoryReader;
    m_impl.reset(new CDianaModuleImpl_Executable(offset, pMemoryReader));
}

void CDianaModule::Analyze(int analyserFlags)
{
    m_impl->Analyze(analyserFlags);
}

void CDianaModule::AnalyzeWithHints(int analyserFlags,
                                    const std::vector<Address_type>& extraEntryPoints)
{
    m_impl->AnalyzeWithHints(analyserFlags, extraEntryPoints);
}

orthia::PlatformString_type CDianaModule::GetName() const
{
    orthia::PlatformStringStream_type str;
    std::hex(str);
    str<<std::setfill(ORTHIA_TCSTR('0')) << std::setw(m_impl->GetDianaMode()*2) <<m_offset;
    return str.str();
}
void CDianaModule::QueryInstructionIterator(CDianaInstructionIterator * pIterator)
{
    pIterator->Init(this);
}
// Iterator
CDianaInstructionIterator::CDianaInstructionIterator()
    :
        m_pModule(0),
        m_currentInstruction(0)
{
}
void CDianaInstructionIterator::Init(CDianaModule * pModule)
{
    m_pModule = pModule;
    MoveToFirst();
}
void CDianaInstructionIterator::SkipExternals()
{
    for(;;++m_currentInstruction)
    {
        if (IsEmpty())
            break;
        Diana_Instruction * pInstruction = &m_pModule->m_impl->m_owner.m_pInstructionsVec[m_currentInstruction];
        // a rolled back route stays in the vector: it is no code
        if (!(pInstruction->m_flags & (DI_INSTRUCTION_EXTERNAL | DI_INSTRUCTION_INVALID)))
            break;
    }
}
void CDianaInstructionIterator::MoveToFirst()
{
    m_currentInstruction = 0;
    SkipExternals();
}
void CDianaInstructionIterator::MoveToNext()
{
    ++m_currentInstruction;
    SkipExternals();
}
bool CDianaInstructionIterator::IsEmpty() const
{
    // m_actualSize counts the valid instructions only, the vector has m_usedSize
    if (m_currentInstruction >= m_pModule->m_impl->m_owner.m_usedSize)
        return true;
    return false;
}
Address_type CDianaInstructionIterator::GetInstructionOffset()
{
    if (IsEmpty())
        throw std::runtime_error("Internal error");

    return m_pModule->m_impl->m_owner.m_pInstructionsVec[m_currentInstruction].m_offset;
}
void CDianaInstructionIterator::QueryRefsToCurrentInstuction(std::vector<CommonReferenceInfo> * pInfo)
{
    if (IsEmpty())
        throw std::runtime_error("Internal error");

    Diana_Instruction * pInstruction = &m_pModule->m_impl->m_owner.m_pInstructionsVec[m_currentInstruction];
    const Diana_FList_List * pRefs = &pInstruction->m_referencesToThisInstruction;
    Diana_SubXRef * pSubRef = (Diana_SubXRef * )Diana_FList_First(pRefs);
    pInfo->clear();
    while(pSubRef)
    {
        Diana_XRef * pCurXRef = Diana_CastXREF(&pSubRef->m_instructionEntry, 0);
        // from a rolled back route
        if (!(pCurXRef->m_flags & DI_XREF_INVALID))
        {
            pInfo->push_back(CommonReferenceInfo(pCurXRef->m_subrefs[0].m_pInstruction->m_offset,
                                    (pCurXRef->m_flags & DI_XREF_EXTERNAL)?true:false));
        }
        pSubRef = (Diana_SubXRef *)Diana_FList_Next(pRefs, &pSubRef->m_instructionEntry);
    }
}
void CDianaInstructionIterator::QueryRefsFromCurrentInstruction(std::vector<CommonReferenceInfo> * pInfo)
{
    if (IsEmpty())
        throw std::runtime_error("Internal error");

    Diana_Instruction * pInstruction = &m_pModule->m_impl->m_owner.m_pInstructionsVec[m_currentInstruction];
    const Diana_FList_List * pRefs = &pInstruction->m_referencesFromThisInstruction;
    Diana_SubXRef * pSubRef = (Diana_SubXRef * )Diana_FList_First(pRefs);
    pInfo->clear();
    while(pSubRef)
    {
        Diana_XRef * pCurXRef = Diana_CastXREF(&pSubRef->m_instructionEntry, 1);     
        if ((pCurXRef->m_flags & DI_XREF_EXTERNAL) && !(pCurXRef->m_flags & DI_XREF_INVALID))
        {
            // save only externals to avoid duplicate with save_refs_to
            pInfo->push_back(CommonReferenceInfo(pCurXRef->m_subrefs[1].m_pInstruction->m_offset, true));
        }
        pSubRef = (Diana_SubXRef *)Diana_FList_Next(pRefs, &pSubRef->m_instructionEntry);
    }

}

}