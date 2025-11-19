#ifndef ORTHIA_DATABASE_IMPL_CLASSIC_H
#define ORTHIA_DATABASE_IMPL_CLASSIC_H

#include "orthia_utils.h"
#include "orthia_interfaces.h"
#include "orthia_sqlite_utils.h"
#include <functional>

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
    orthia::CCriticalSection m_lock;
    intrusive_ptr<CSQLDatabase2> m_pDatabase;
    std::set<Address_type> m_cache;
    CClassicDatabase(const CClassicDatabase &);
    CClassicDatabase & operator =(const CClassicDatabase &);

    CSQLStatement m_stmtInsertReferences;
    CSQLStatement m_stmtSelectReferencesTo;
    CSQLStatement m_stmtSelectReferencesFrom;

    CSQLStatement m_stmtSelectModule;
    CSQLStatement m_stmtQueryModules;
    CSQLStatement m_stmtQueryModuleById, m_stmtQueryNearestModuleById;

    CSQLStatement m_stmtSelectReferencesToRange;
    CSQLStatement m_stmtSelectReferencesFromRange;

    CSQLStatement m_stmtQueryRouteStart;

    CSQLStatement m_stmtInsertMetainfo;
    CSQLStatement m_stmtInsertModule;

    CSQLStatement m_stmtSelectMetainfo_All;
    CSQLStatement m_stmtSelectMetainfo_Module2;
    CSQLStatement m_stmtSelectMetainfo_Module2_Count;
    CSQLStatement m_stmtSelectMetainfo_Address;
    CSQLStatement m_stmtSelectMetainfo_NearestAddress;

    CSQLStatement m_stmtSelectAllComments;
    CSQLStatement m_stmtWriteComment;

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

    void InsertMetaInfo(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress = DI_MAX_OPERAND_SIZE);
    void QueryMetaInfo(int metaType, std::function<bool (Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler);
    void QueryMetaInfoModule2(Address_type moduleAddress, int metaType1, int metaType2, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler);
    int QueryMetaInfoModule2_Count(Address_type moduleAddress, int metaType1, int metaType2);
    void QueryMetaInfoByAddress(int metaType, Address_type metaAddress, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler);
    void QueryMetaInfoByNearestAddress(int metaType, Address_type address, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler);

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
    bool QueryModule(Address_type moduleAddress, CommonModuleInfo * pResult);
    bool QueryNearestModule(Address_type address, CommonModuleInfo* pResult);

    void RollbackTransactionSilent();
    
    // comments
    void InsertComment(Address_type address, const std::string& text);
    void QueryAllComments(std::function<bool(Address_type address, const std::string& text)> handler);
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
