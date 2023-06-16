#include "orthia_module_manager.h"
#include "orthia_database_saver.h"

namespace orthia
{

CModuleManager::CModuleManager()
{
}
void CModuleManager::Reinit(const std::wstring & fullFileName,
                            bool bForce)
{
    CAutoCriticalSection guard(m_lock);
    orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager(new CDatabaseManager());
    if (bForce || (!orthia::IsFileExist(fullFileName)) || !orthia::GetSizeOfFile(fullFileName))
    {
        pDatabaseManager->CreateNew(fullFileName);
    }
    else
    {
        pDatabaseManager->OpenExisting(fullFileName);
    }
    m_pDatabaseManager = pDatabaseManager;
    m_fullFileName = fullFileName;
}
orthia::intrusive_ptr<CDatabaseManager> CModuleManager::QueryDatabaseManager()
{
    CAutoCriticalSection guard(m_lock);
    if (!m_pDatabaseManager)
    {
        throw std::runtime_error("The profile is not initialized");
    }
    return m_pDatabaseManager;
}
std::wstring CModuleManager::GetDatabaseName() const
{
    return m_fullFileName;
}

void CModuleManager::UnloadModule(Address_type offset)
{
    CAutoCriticalSection guard(m_lock);
    m_pDatabaseManager->GetClassicDatabase()->UnloadModule(offset, false);
}

void CModuleManager::ReloadRange(Address_type offset,
                                 Address_type size,
                                 IMemoryReader * pMemoryReader,
                                 int mode,
                                 int analyserFlags)
{
    CAutoCriticalSection guard(m_lock);
    std::wstringstream regionName;
    std::hex(regionName);
    regionName<<L"region_"<<offset<<"_"<<size;

    CDianaModule module;
    module.InitRaw(offset, size, pMemoryReader, mode);

    if (m_pDatabaseManager->GetClassicDatabase()->IsModuleExists(offset))
    {
        std::stringstream errorStream;
        std::hex(errorStream);
        errorStream<<"The region already exists: "<<offset;
        throw std::runtime_error(errorStream.str());
    }

    module.Analyze(analyserFlags);

    CDatabaseSaver fileSaver;
    fileSaver.Save(module, *m_pDatabaseManager, regionName.str());
}

void CModuleManager::ReloadModule(Address_type offset,
                                  IMemoryReader * pMemoryReader,
                                  bool bForce,
                                  const std::wstring & name,
                                  int analyserFlags)
{
    CAutoCriticalSection guard(m_lock);
    CDianaModule module;
    module.Init(offset, pMemoryReader);

    if (!m_pDatabaseManager)
        throw std::runtime_error("The profile is not initialized");
    
    // build full path name
    if (bForce || !m_pDatabaseManager->GetClassicDatabase()->IsModuleExists(offset))
    {
        module.Analyze(analyserFlags);
        m_pDatabaseManager->GetClassicDatabase()->UnloadModule(offset, true);
        
        CDatabaseSaver fileSaver;
        fileSaver.Save(module, *m_pDatabaseManager, name);
    }
}

// module info
void CModuleManager::QueryLoadedModules(std::vector<CommonModuleInfo> * pResult) const
{
    CAutoCriticalSection guard(m_lock);
    m_pDatabaseManager->GetClassicDatabase()->QueryModules(pResult);
}
// references
void CModuleManager::QueryReferencesToInstruction(Address_type offset, 
                                                 std::vector<CommonReferenceInfo> * pResult) const
{
    CAutoCriticalSection guard(m_lock);
    m_pDatabaseManager->GetClassicDatabase()->QueryReferencesToInstruction(offset, pResult);
}
void CModuleManager::QueryReferencesFromInstruction(Address_type offset, 
                                                 std::vector<CommonReferenceInfo> * pResult) const
{
    CAutoCriticalSection guard(m_lock);
    m_pDatabaseManager->GetClassicDatabase()->QueryReferencesFromInstruction(offset, pResult);
}
void CModuleManager::QueryReferencesToInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult) const
{
    CAutoCriticalSection guard(m_lock);
    if (address1 > address2)
    {
        throw std::runtime_error("Invalid arguments");
    }
    m_pDatabaseManager->GetClassicDatabase()->QueryReferencesToInstructionsRange(address1, address2, pResult);
}
void CModuleManager::QueryReferencesFromInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult) const
{
    CAutoCriticalSection guard(m_lock);
    m_pDatabaseManager->GetClassicDatabase()->QueryReferencesFromInstructionsRange(address1, address2, pResult);
}

}