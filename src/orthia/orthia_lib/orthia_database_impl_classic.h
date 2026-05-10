#ifndef ORTHIA_DATABASE_IMPL_CLASSIC_H
#define ORTHIA_DATABASE_IMPL_CLASSIC_H

#include "orthia_utils.h"
#include "orthia_interfaces.h"
#include "orthia_sqlite_utils.h"
#include <functional>
#include <memory>

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

class CClassicDatabaseBatch
{
    CClassicDatabase* m_db;
    bool m_committed;
    orthia::CAutoCriticalSection m_guard;

    CClassicDatabaseBatch(const CClassicDatabaseBatch&) = delete;
    CClassicDatabaseBatch& operator=(const CClassicDatabaseBatch&) = delete;
    explicit CClassicDatabaseBatch(CClassicDatabase* db);
    friend class CClassicDatabase;
public:
    void Commit();
    ~CClassicDatabaseBatch();
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
    CSQLStatement m_stmtUpdateMetainfo;
    CSQLStatement m_stmtInsertModule;

    CSQLStatement m_stmtSelectMetainfo_All;
    CSQLStatement m_stmtSelectMetainfo_Module2;
    CSQLStatement m_stmtSelectMetainfo_Module2_FromAddress;
    CSQLStatement m_stmtSelectMetainfo_Module2_Count;
    CSQLStatement m_stmtSelectMetainfo_Address;
    CSQLStatement m_stmtSelectMetainfo_NearestAddress;
    CSQLStatement m_stmtSelectMetainfo_NearestAddress2;
    CSQLStatement m_stmtSelectMetainfo_AddressRange;

    CSQLStatement m_stmtSelectAllComments;
    CSQLStatement m_stmtWriteComment;

    void InsertReference(sqlite3_stmt * stmt, Address_type from, Address_type to);
    void InsertModule(Address_type baseAddress, Address_type size, const orthia::PlatformString_type & moduleName);
    void BeginBatchInsert();   // no lock -- called with m_lock held by CClassicDatabaseBatch
    void CommitBatchInsert();  // no lock -- called with m_lock held by CClassicDatabaseBatch
    friend class CClassicDatabaseBatch;

    void Init();

public:
    CClassicDatabase(intrusive_ptr<CSQLDatabase2> pDatabase);
    ~CClassicDatabase();

    orthia::CCriticalSection& GetLock() { return m_lock; }

    // module loading process:
    void StartSaveModule(Address_type baseAddress,
                         Address_type size,
                         const orthia::PlatformString_type & moduleName,
                         CAutoRollbackClassicDatabase * pRollback,
                         bool replaceExisting);
    void DoneSave();
    void CleanupResources();

    std::unique_ptr<CClassicDatabaseBatch> BeginBatch();

    void InsertReferencesToInstruction(Address_type offset, const std::vector<CommonReferenceInfo> & references);
    void InsertReferencesFromInstruction(Address_type offset, const std::vector<CommonReferenceInfo> & references);

    void InsertMetaInfo(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress = DI_MAX_OPERAND_SIZE, bool checkDuplicates = false);
    void QueryMetaInfo(int metaType, std::function<bool (Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler);
    void QueryMetaInfoModule2(Address_type moduleAddress, int metaType1, int metaType2, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler, Address_type addressHint = 0);
    int QueryMetaInfoModule2_Count(Address_type moduleAddress, int metaType1, int metaType2);
    void QueryMetaInfoByAddress(int metaType, Address_type metaAddress, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler);
    void QueryMetaInfoByNearestAddress(int metaType, Address_type address, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler, int metaType2 = -1);
    void QueryMetaInfoByAddressRange(int metaType, Address_type addr1, Address_type addr2, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler);
    void QueryMetaInfoByAddressRange2(int metaType1, int metaType2,  Address_type addr1, Address_type addr2, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler);

    // queries
    Address_type QueryRouteStart(Address_type offset);
    void QueryReferencesFromInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pReferences);
    void QueryReferencesToInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pReferences);
    void QueryReferencesToInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult);
    void QueryReferencesFromInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult);
    void QueryAllModuleReferences(Address_type baseAddress, Address_type size,
        std::vector<std::pair<Address_type, Address_type>>& result);

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
