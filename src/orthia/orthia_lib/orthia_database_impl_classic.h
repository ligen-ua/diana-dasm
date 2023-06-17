#ifndef ORTHIA_DATABASE_IMPL_CLASSIC_H
#define ORTHIA_DATABASE_IMPL_CLASSIC_H

#include "orthia_utils.h"
#include "orthia_interfaces.h"
#include "orthia_sqlite_utils.h"


namespace orthia
{

struct CommonReferenceInfo;

class CClassicDatabase;
class CAutoRollbackClassicDatabase
{
    CClassicDatabase * m_pDatabase;
public:
    CAutoRollbackClassicDatabase();
    void Init(CClassicDatabase * pDatabase);
    void Reset();
    ~CAutoRollbackClassicDatabase();
};

class CClassicDatabase:public orthia::RefCountedBase
{
    intrusive_ptr<CSQLDatabase2> m_pDatabase;
    std::set<Address_type> m_cache;
    CClassicDatabase(const CClassicDatabase &);
    CClassicDatabase & operator =(const CClassicDatabase &);

    CSQLStatement m_stmtInsertReferences;
    CSQLStatement m_stmtSelectReferencesTo;
    CSQLStatement m_stmtSelectReferencesFrom;

    CSQLStatement m_stmtSelectModule;
    CSQLStatement m_stmtQueryModules;
    CSQLStatement m_stmtSelectReferencesToRange;
    CSQLStatement m_stmtSelectReferencesFromRange;

    CSQLStatement m_stmtQueryRouteStart;

    void InsertReference(sqlite3_stmt * stmt, Address_type from, Address_type to);
    void InsertModule(Address_type baseAddress, Address_type size, const std::wstring & moduleName);

    void Init();

public:
    CClassicDatabase(intrusive_ptr<CSQLDatabase2> pDatabase);
    ~CClassicDatabase();

    // module loading process:
    void StartSaveModule(Address_type baseAddress, 
                         Address_type size, 
                         const std::wstring & moduleName,
                         CAutoRollbackClassicDatabase * pRollback);
    void DoneSave();
    void CleanupResources();

    void InsertReferencesToInstruction(Address_type offset, const std::vector<CommonReferenceInfo> & references);
    void InsertReferencesFromInstruction(Address_type offset, const std::vector<CommonReferenceInfo> & references);

    // queries
    Address_type QueryRouteStart(Address_type offset);
    void QueryReferencesFromInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pReferences);
    void QueryReferencesToInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pReferences);
    void QueryReferencesToInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult);
    void QueryReferencesFromInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult);

    // modules api
    void UnloadModule(Address_type address, bool bSilent);
    bool IsModuleExists(Address_type address);
    void QueryModules(std::vector<CommonModuleInfo> * pResult);
    void RollbackTransactionSilent();
};

class CClassicDatabaseModuleCleaner
{
    CClassicDatabase * m_pDatabaseModule;
    CClassicDatabaseModuleCleaner(const CClassicDatabaseModuleCleaner&);
    CClassicDatabaseModuleCleaner & operator = (const CClassicDatabaseModuleCleaner&);
public:
    CClassicDatabaseModuleCleaner(CClassicDatabase * pDatabaseModule)
        :
            m_pDatabaseModule(pDatabaseModule)
    {
    }
    ~CClassicDatabaseModuleCleaner()
    {
        m_pDatabaseModule->CleanupResources();
    }
};

ORTHIA_DECLARE(CClassicDatabase);

}
#endif
