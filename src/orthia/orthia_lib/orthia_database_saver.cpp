#include "orthia_database_saver.h"
#include "orthia_diana_module.h"

namespace orthia
{

CDatabaseSaver::CDatabaseSaver()
{
}

void CDatabaseSaver::Save(CDianaModule & dianaModule,
                          CDatabaseManager & databaseManager,
                          const orthia::PlatformString_type & moduleName,
                          bool replaceModule)
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
    classicDatabase->StartSaveModule(baseAddress, dianaModule.GetModuleSize(), moduleName, &rollback, replaceModule);
 
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

void CDatabaseSaver::SaveWithDedup(CDianaModule & dianaModule,
                                   CDatabaseManager & databaseManager,
                                   const orthia::PlatformString_type & moduleName)
{
    using XRefPair = std::pair<Address_type, Address_type>;

    orthia::intrusive_ptr<CDatabase> databaseModule = databaseManager.GetDatabase();
    orthia::intrusive_ptr<CClassicDatabase> classicDatabase = databaseModule->GetClassicDatabase();

    CAutoCriticalSection guard(classicDatabase->GetLock());

    Address_type baseAddress = dianaModule.GetModuleAddress();
    std::vector<CommonReferenceInfo> references;
    CDianaInstructionIterator iterator;
    dianaModule.QueryInstructionIterator(&iterator);

    CClassicDatabaseModuleCleaner cleaner(classicDatabase.get());
    CAutoRollbackClassicDatabase rollback;
    classicDatabase->StartSaveModule(baseAddress, dianaModule.GetModuleSize(), moduleName, &rollback, false);

    // Load existing xrefs. byFrom sorted by (from,to) from DB ORDER BY.
    // byTo is the same pairs keyed as (to,from) and sorted — for "to-ref" lookups.
    // Instructions are iterated in discovery order (not address order), so we use
    // lower_bound for O(log N) random access rather than a monotone cursor.
    std::vector<XRefPair> byFrom;
    classicDatabase->QueryAllModuleReferences(baseAddress, dianaModule.GetModuleSize(), byFrom);

    std::vector<XRefPair> byTo;
    byTo.reserve(byFrom.size());
    for (const auto& p : byFrom)
        byTo.push_back({p.second, p.first});
    std::sort(byTo.begin(), byTo.end());

    auto xrefExistsTo   = [&](Address_type to,   Address_type from) {
        XRefPair key{to, from};
        auto it = std::lower_bound(byTo.cbegin(), byTo.cend(), key);
        return it != byTo.cend() && *it == key;
    };
    auto xrefExistsFrom = [&](Address_type from, Address_type to) {
        XRefPair key{from, to};
        auto it = std::lower_bound(byFrom.cbegin(), byFrom.cend(), key);
        return it != byFrom.cend() && *it == key;
    };

    std::vector<CommonReferenceInfo> filtered; // reused across iterations

    for (int i = 0; !iterator.IsEmpty(); ++i)
    {
        Address_type offset    = iterator.GetInstructionOffset();
        Address_type absOffset = offset + baseAddress;

        // --- "to" refs: rows inserted as (ref.address → absOffset) ---
        iterator.QueryRefsToCurrentInstuction(&references);
        if (!references.empty())
        {
            for (auto& r : references)
                if (!r.external) r.address += baseAddress;

            filtered.clear();
            for (const auto& r : references)
            {
                if (!xrefExistsTo(absOffset, r.address))
                    filtered.push_back(r);
            }
            if (!filtered.empty())
                classicDatabase->InsertReferencesToInstruction(absOffset, filtered);
        }

        // --- "from" refs: rows inserted as (absOffset → ref.address) ---
        iterator.QueryRefsFromCurrentInstruction(&references);
        if (!references.empty())
        {
            filtered.clear();
            for (const auto& r : references)
            {
                if (!xrefExistsFrom(absOffset, r.address))
                    filtered.push_back(r);
            }
            if (!filtered.empty())
                classicDatabase->InsertReferencesFromInstruction(absOffset, filtered);
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