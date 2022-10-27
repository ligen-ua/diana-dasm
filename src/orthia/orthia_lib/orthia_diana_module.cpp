#include "orthia_diana_module.h"
#include "orthia_memory_cache.h"
#include <iomanip>
#include "orthia_sha1.h"

extern "C"
{
#include "diana_pe_analyzer.h"
}
#include "diana_core_cpp.h"
#include "orthia_streams.h"

namespace orthia
{

int DianaEnvironment_ConvertAddressToRelative(void * pThis, 
                                              OPERAND_SIZE address,              
                                              OPERAND_SIZE * pRelativeOffset,
                                              int * pbInvalidPointer);

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
            ULARGE_INTEGER temp;
            temp.QuadPart = address;
            temp.HighPart = (ULONG)-1;
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

class CDianaModuleImpl
{
protected:
    friend class CDianaInstructionIterator;
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
    orthia::Address_type GetModuleSize() { return m_env.m_moduleSize; }
    virtual int GetDianaMode() const =0; 
};

class CDianaModuleImpl_PE:public CDianaModuleImpl
{
    Diana_PeFile m_peFile;
    diana::Guard<diana::PeFile> m_peFileGuard;
protected:
    void AnalyzeImpl(int analyserFlags)
    {
        m_cache.Init(m_env.m_moduleStart, 
                     m_env.m_moduleSize,
                     m_peFile.pImpl->pCapturedSections,
                     m_peFile.pImpl->capturedSectionCount); 
        DI_CHECK_CPP(Diana_PE_AnalyzePE(&m_peFile, 
                                        &m_env, 
                                        &m_owner,
                                        analyserFlags));
        m_instructionsOwnerGuard.reset(&m_owner);
    }
public:
    CDianaModuleImpl_PE(Address_type offset,
                       IMemoryReader * pMemoryReader)
           :
            CDianaModuleImpl(offset, pMemoryReader)
    {
        DI_CHECK_CPP(DianaPeFile_Init(&m_peFile, 
                                      &m_env.m_stream.parent, 
                                      0,
                                      DIANA_PE_FILE_FLAGS_MODULE_MODE));
        m_peFileGuard.reset(&m_peFile);
        InitEnv(m_peFile.pImpl->sizeOfModule, GetPeFile()->pImpl->dianaMode);
    }
    ~CDianaModuleImpl_PE()
    {
    }

    const Diana_PeFile * GetPeFile() const { return &m_peFile; }
    int GetDianaMode() const { return GetPeFile()->pImpl->dianaMode; }
};

class CDianaModuleImpl_Range:public CDianaModuleImpl
{
    int m_dianaMode;
protected:
    void AnalyzeImpl(int analyserFlags)
    {
        &analyserFlags;
        m_cache.Init(m_env.m_moduleStart, 
                     m_env.m_moduleSize); 
        DI_CHECK_CPP(Diana_InstructionsOwner_Init(&m_owner, 
                                              m_env.m_moduleSize, 
                                              0x10000));
        m_instructionsOwnerGuard.reset(&m_owner);
        DI_CHECK_CPP(Diana_AnalyzeCode(&m_owner, &m_env, m_dianaMode, 0, m_env.m_moduleSize));
    }
public:
    CDianaModuleImpl_Range(Address_type offset,
                           Address_type size,
                           IMemoryReader * pMemoryReader,
                           int dianaMode)
           :
            CDianaModuleImpl(offset, pMemoryReader),
            m_dianaMode(dianaMode)
    {
        InitEnv(size, dianaMode);
    }
    int GetDianaMode() const { return m_dianaMode; }
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
void CDianaModule::InitRaw(Address_type offset,
                           Address_type size,
                           IMemoryReader * pMemoryReader,
                           int mode)
{
    m_offset = offset;
    m_pMemoryReader = pMemoryReader;
    m_impl.reset(new CDianaModuleImpl_Range(offset, size, pMemoryReader, mode));
}
void CDianaModule::Init(Address_type offset,
                        IMemoryReader * pMemoryReader)
{
    m_offset = offset;
    m_pMemoryReader = pMemoryReader;
    m_impl.reset(new CDianaModuleImpl_PE(offset, pMemoryReader));
}

void CDianaModule::Analyze(int analyserFlags)
{
    m_impl->Analyze(analyserFlags);
}

std::wstring CDianaModule::GetName() const
{
    std::wstringstream str;
    std::hex(str);
    str<<std::setfill(L'0') << std::setw(m_impl->GetDianaMode()*2) <<m_offset;
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
        if (!(pInstruction->m_flags & DI_INSTRUCTION_EXTERNAL))
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
    if (m_currentInstruction >= m_pModule->m_impl->m_owner.m_actualSize)
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
    Diana_SubXRef * pSubRef = (Diana_SubXRef * )pInstruction->m_referencesToThisInstruction.m_pFirst;
    pInfo->clear();
    while(pSubRef)
    {
        Diana_XRef * pCurXRef = Diana_CastXREF(&pSubRef->m_instructionEntry, 0);     
        pInfo->push_back(CommonReferenceInfo(pCurXRef->m_subrefs[0].m_pInstruction->m_offset, 
                                (pCurXRef->m_flags & DI_XREF_EXTERNAL)?true:false));
        pSubRef = (Diana_SubXRef *)pSubRef->m_instructionEntry.m_pNext;
    }    
}
void CDianaInstructionIterator::QueryRefsFromCurrentInstruction(std::vector<CommonReferenceInfo> * pInfo)
{
    if (IsEmpty())
        throw std::runtime_error("Internal error");

    Diana_Instruction * pInstruction = &m_pModule->m_impl->m_owner.m_pInstructionsVec[m_currentInstruction];
    Diana_SubXRef * pSubRef = (Diana_SubXRef * )pInstruction->m_referencesFromThisInstruction.m_pFirst;
    pInfo->clear();
    while(pSubRef)
    {
        Diana_XRef * pCurXRef = Diana_CastXREF(&pSubRef->m_instructionEntry, 1);     
        if (pCurXRef->m_flags & DI_XREF_EXTERNAL)
        {
            // save only externals to avoid duplicate with save_refs_to
            pInfo->push_back(CommonReferenceInfo(pCurXRef->m_subrefs[1].m_pInstruction->m_offset, true));
        }
        pSubRef = (Diana_SubXRef *)pSubRef->m_instructionEntry.m_pNext;
    }

}

}