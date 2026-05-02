#include "orthia_database_saver.h"
#include "orthia_diana_module.h"

namespace orthia
{

CDatabaseSaver::CDatabaseSaver()
{
}

void CDatabaseSaver::Save(CDianaModule & dianaModule,
                          CDatabaseManager & databaseManager,
                          const orthia::PlatformString_type & moduleName)
{
    orthia::intrusive_ptr<CDatabase> databaseModule = databaseManager.GetDatabase();
    orthia::intrusive_ptr<CClassicDatabase> classicDatabase = databaseModule->GetClassicDatabase();
    
    CAutoCriticalSection guard(classicDatabase->GetLock());
    
    Address_type baseAddress = dianaModule.GetModuleAddress();
    std::vector<CommonReferenceInfo> references;
    CDianaInstructionIterator iterator;
    dianaModule.QueryInstructionIterator(&iterator);

    CClassicDatabaseModuleCleaner cleaner(classicDatabase.get());
    CAutoRollbackClassicDatabase rollback;
    classicDatabase->StartSaveModule(baseAddress, dianaModule.GetModuleSize(), moduleName, &rollback);
 
    for(int i = 0; !iterator.IsEmpty(); ++i)
    {
        Address_type offset = iterator.GetInstructionOffset();
        iterator.QueryRefsToCurrentInstuction(&references);
        if (!references.empty())
        {
            // diana returns relative offsets, convert it to the absolute ones
            for(std::vector<CommonReferenceInfo>::iterator it = references.begin(), it_end = references.end();
                it != it_end; ++it)
            {
                if (!it->external)
                {
                    it->address += baseAddress;
                }
            }
            classicDatabase->InsertReferencesToInstruction(offset+baseAddress, references);
        }
        iterator.QueryRefsFromCurrentInstruction(&references);
        if (!references.empty())
        {
            classicDatabase->InsertReferencesFromInstruction(offset+baseAddress, references);    
        }
        iterator.MoveToNext();
        if (!(i % 10))
        {
            classicDatabase->GetLock().Unlock();
            classicDatabase->GetLock().Lock();
        }
    }
    classicDatabase->DoneSave();
    rollback.Reset();
}

}