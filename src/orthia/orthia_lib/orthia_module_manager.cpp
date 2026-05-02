#include "orthia_module_manager.h"
#include "orthia_database_saver.h"

namespace orthia
{

CModuleManager::CModuleManager()
{
}
void CModuleManager::Reinit(const orthia::PlatformString_type & fullFileName,
                            bool bForce)
{
    CAutoCriticalSection guard(m_writeLock);
    CAutoCriticalSection guard2(m_dbLock);
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
orthia::intrusive_ptr<CDatabaseManager> CModuleManager::QueryDatabaseManager() const
{
    CAutoCriticalSection guard(m_dbLock);
    if (!m_pDatabaseManager)
    {
        throw std::runtime_error("The profile is not initialized");
    }
    return m_pDatabaseManager;
}
orthia::PlatformString_type CModuleManager::GetDatabaseName() const
{
    return m_fullFileName;
}

void CModuleManager::UnloadModule(Address_type offset)
{
    CAutoCriticalSection guard(m_writeLock);
    QueryDatabaseManager()->GetClassicDatabase()->UnloadModule(offset, false);
}

void CModuleManager::ReloadRange(Address_type offset,
                                 Address_type size,
                                 IMemoryReader * pMemoryReader,
                                 int mode,
                                 int analyserFlags)
{
    CAutoCriticalSection guard(m_writeLock);
    orthia::PlatformStringStream_type regionName;
    std::hex(regionName);
    regionName<<ORTHIA_TCSTR("region_")<<offset<<ORTHIA_TCSTR("_")<<size;

    CDianaModule module;
    module.InitRaw(offset, size, pMemoryReader, mode);

    if (QueryDatabaseManager()->GetClassicDatabase()->IsModuleExists(offset))
    {
        std::stringstream errorStream;
        std::hex(errorStream);
        errorStream<<"The region already exists: "<<offset;
        throw std::runtime_error(errorStream.str());
    }

    module.Analyze(analyserFlags);

    CDatabaseSaver fileSaver;
    fileSaver.Save(module, *QueryDatabaseManager(), regionName.str());
}

void CModuleManager::ReloadModule(Address_type offset,
                                  IMemoryReader * pMemoryReader,
                                  bool bForce,
                                  const orthia::PlatformString_type & name,
                                  int analyserFlags)
{
    CAutoCriticalSection guard(m_writeLock);

    CDianaModule module;
    module.Init(offset, pMemoryReader);

    auto dbManager = QueryDatabaseManager();
    
    // build full path name
    if (bForce || !dbManager->GetClassicDatabase()->IsModuleExists(offset))
    {
        module.Analyze(analyserFlags);

        dbManager->GetClassicDatabase()->UnloadModule(offset, true);
        
        CDatabaseSaver fileSaver;
        fileSaver.Save(module, *dbManager, name);
    }
}

// module info
void CModuleManager::QueryLoadedModules(std::vector<CommonModuleInfo> * pResult) const
{
    m_pDatabaseManager->GetClassicDatabase()->QueryModules(pResult);
}
// references
Address_type CModuleManager::QueryRouteStart(Address_type offset)
{
    return m_pDatabaseManager->GetClassicDatabase()->QueryRouteStart(offset);
}
void CModuleManager::QueryReferencesToInstruction(Address_type offset, 
                                                 std::vector<CommonReferenceInfo> * pResult) const
{
    QueryDatabaseManager()->GetClassicDatabase()->QueryReferencesToInstruction(offset, pResult);
}
void CModuleManager::QueryReferencesFromInstruction(Address_type offset, 
                                                 std::vector<CommonReferenceInfo> * pResult) const
{
    QueryDatabaseManager()->GetClassicDatabase()->QueryReferencesFromInstruction(offset, pResult);
}
void CModuleManager::QueryReferencesToInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult) const
{
    if (address1 > address2)
    {
        throw std::runtime_error("Invalid arguments");
    }
    QueryDatabaseManager()->GetClassicDatabase()->QueryReferencesToInstructionsRange(address1, address2, pResult);
}
void CModuleManager::QueryReferencesFromInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult) const
{
    QueryDatabaseManager()->GetClassicDatabase()->QueryReferencesFromInstructionsRange(address1, address2, pResult);
}

}