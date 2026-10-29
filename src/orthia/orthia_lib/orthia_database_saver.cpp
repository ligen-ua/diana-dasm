#include "orthia_database_saver.h"
#include "orthia_diana_module.h"
#include <algorithm>

namespace orthia
{

// AArch32: where the code changes between A32, T32 and data, for the disassembly
static void SaveArmCodeMap(CDianaModule & dianaModule, CClassicDatabase & classicDatabase, Address_type baseAddress)
{
    std::vector<ArmCodeChange> changes;
    dianaModule.QueryArmCodeMap(changes);
    for (const auto& change : changes)
    {
        classicDatabase.InsertMetaInfo(baseAddress, g_database_type_armCodeMap, GetArmCodeKindName(change.kind),
            baseAddress + change.offset, true);
    }
}

CDatabaseSaver::CDatabaseSaver(ExternalTargetFilter_type externalTargetFilter)
    :
        m_externalTargetFilter(std::move(externalTargetFilter))
{
}

// only taken addresses: calls, jumps and data accesses out of the module are what the code does
bool CDatabaseSaver::IsDropped(const CDianaModule & dianaModule, const CommonReferenceInfo & ref) const
{
    if (!m_externalTargetFilter || ref.kind != reference_address)
        return false;
    if (ref.address - dianaModule.GetModuleAddress() < dianaModule.GetModuleSize())
        return false;
    return !m_externalTargetFilter(ref.address);
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
    auto isDropped = [&](const CommonReferenceInfo & ref) { return IsDropped(dianaModule, ref); };

    CClassicDatabaseModuleCleaner cleaner(classicDatabase.get());
    CAutoRollbackClassicDatabase rollback;
    classicDatabase->StartSaveModule(baseAddress, dianaModule.GetModuleSize(), moduleName, &rollback, replaceModule);
 
    for(int i = 0; !iterator.IsEmpty(); ++i)
    {
        Address_type offset = iterator.GetInstructionOffset();
        iterator.QueryRefsToCurrentInstuction(&references);
        if (!references.empty())
        {
            classicDatabase->InsertReferencesToInstruction(offset+baseAddress, references);
        }
        iterator.QueryRefsFromCurrentInstruction(&references);
        references.erase(std::remove_if(references.begin(), references.end(), isDropped), references.end());
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
    SaveArmCodeMap(dianaModule, *classicDatabase, baseAddress);
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
                if (!xrefExistsFrom(absOffset, r.address) && !IsDropped(dianaModule, r))
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
    SaveArmCodeMap(dianaModule, *classicDatabase, baseAddress);
    classicDatabase->DoneSave();
    rollback.Reset();
}

}