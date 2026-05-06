#include "orthia_database_impl_classic.h"

namespace orthia
{

static inline sqlite3_int64 AddrToDb(orthia::Address_type addr)
{
    return (sqlite3_int64)(addr ^ 0x8000000000000000ULL);
}
static inline orthia::Address_type DbToAddr(sqlite3_int64 val)
{
    return (orthia::Address_type)(val) ^ 0x8000000000000000ULL;
}

CAutoRollbackClassicDatabase::CAutoRollbackClassicDatabase()
    :
        m_pDatabase(0)
{
}
void CAutoRollbackClassicDatabase::Init(CClassicDatabase * pDatabase)
{
    m_pDatabase = pDatabase;
}
void CAutoRollbackClassicDatabase::Reset()
{
    m_pDatabase = 0;
}
CAutoRollbackClassicDatabase::~CAutoRollbackClassicDatabase()
{
    if (m_pDatabase)
    {
        m_pDatabase->RollbackTransactionSilent();
    }
}

CClassicDatabase::CClassicDatabase(intrusive_ptr<CSQLDatabase2> pDatabase)
    :
        m_pDatabase(pDatabase)
{
    Init();
}
CClassicDatabase::~CClassicDatabase()
{

}

void CClassicDatabase::Init()
{
    // prepare references
    char * buffer = 0;
    buffer = "SELECT ref_address_from FROM tbl_references WHERE ref_address_to = ?1 ORDER BY ref_address_from";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectReferencesTo.Get2(), NULL));
    buffer = "SELECT * FROM tbl_modules WHERE mod_address = ?1";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectModule.Get2(), NULL));
    
    buffer = "SELECT mod_address, mod_size, mod_name  FROM tbl_modules ORDER BY mod_address";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtQueryModules.Get2(), NULL));
    buffer = "SELECT mod_size, mod_name  FROM tbl_modules WHERE mod_address = ?1";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtQueryModuleById.Get2(), NULL));

    buffer = "SELECT mod_address, mod_size, mod_name  FROM tbl_modules WHERE UINT_LESSOE(mod_address, ?1)";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtQueryNearestModuleById.Get2(), NULL));


    buffer = "SELECT ref_address_to, ref_address_from FROM tbl_references WHERE ref_address_to BETWEEN ?1 AND ?2 ORDER BY ref_address_to, ref_address_from";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectReferencesToRange.Get2(), NULL));
    buffer = "SELECT ref_address_to FROM tbl_references WHERE ref_address_from = ?1 ORDER BY ref_address_to";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectReferencesFrom.Get2(), NULL));

    buffer = "SELECT ref_address_from, ref_address_to FROM tbl_references WHERE ref_address_from BETWEEN ?1 AND ?2 ORDER BY ref_address_from, ref_address_to";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectReferencesFromRange.Get2(), NULL));

    buffer = "SELECT ref_address_to FROM tbl_references WHERE ref_address_to <= ?1 ORDER BY ref_address_to DESC LIMIT 1";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtQueryRouteStart.Get2(), NULL));

    buffer = "INSERT OR REPLACE INTO tbl_metainfo(meta_mod_id, meta_address, meta_type, meta_info) VALUES(?1, ?2, ?3, ?4)";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtInsertMetainfo.Get2(), NULL));

    buffer = "INSERT OR REPLACE INTO tbl_modules (mod_address, mod_size, mod_name) VALUES(?1, ?2, ?3)";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtInsertModule.Get2(), NULL));

    buffer = "INSERT OR REPLACE INTO tbl_references (ref_address_from, ref_address_to) VALUES(?1, ?2)";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtInsertReferences.Get2(), NULL));

    buffer = "SELECT meta_mod_id, meta_address, meta_type, meta_info FROM tbl_metainfo WHERE (meta_type == ?1) ORDER BY meta_mod_id, meta_address";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectMetainfo_All.Get2(), NULL));

    buffer = "SELECT meta_address, meta_type, meta_info FROM tbl_metainfo WHERE ((meta_type == ?2) OR (?3 == -1 OR meta_type == ?3)) AND (meta_mod_id == ?1) ORDER BY meta_type, meta_address";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectMetainfo_Module2.Get2(), NULL));

    buffer = "SELECT meta_address, meta_type, meta_info FROM tbl_metainfo WHERE ((meta_type == ?2) OR (?3 == -1 OR meta_type == ?3)) AND (meta_mod_id == ?1) AND meta_address >= ?4 ORDER BY meta_type, meta_address";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectMetainfo_Module2_FromAddress.Get2(), NULL));

    buffer = "SELECT COUNT(meta_address) FROM tbl_metainfo WHERE ((meta_type == ?2) OR (?3 == -1 OR meta_type == ?3)) AND (meta_mod_id == ?1) ";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectMetainfo_Module2_Count.Get2(), NULL));

    buffer = "SELECT meta_mod_id, meta_type, meta_info FROM tbl_metainfo WHERE (meta_type == ?1) AND (meta_address == ?2) ORDER BY meta_type, meta_address";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectMetainfo_Address.Get2(), NULL));

    buffer = "SELECT meta_mod_id, meta_type, meta_info, meta_address FROM tbl_metainfo WHERE (meta_type == ?1) AND (meta_address <= ?2) ORDER BY meta_address DESC";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectMetainfo_NearestAddress.Get2(), NULL));

    buffer = "SELECT meta_mod_id, meta_type, meta_info, meta_address FROM tbl_metainfo WHERE (meta_type == ?1 OR (?3 != -1 AND meta_type == ?3)) AND (meta_address <= ?2) ORDER BY meta_address DESC";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectMetainfo_NearestAddress2.Get2(), NULL));

    buffer = "SELECT meta_mod_id, meta_type, meta_info, meta_address FROM tbl_metainfo WHERE (meta_type == ?1) AND (meta_address >= ?2) AND (meta_address <= ?3) ORDER BY meta_type, meta_address";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectMetainfo_AddressRange.Get2(), NULL));

    // comments
    buffer = "SELECT com_address, com_text FROM tbl_comments ORDER BY com_address";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectAllComments.Get2(), NULL));

    buffer = "INSERT OR REPLACE INTO tbl_comments(com_address, com_text) VALUES(?1, ?2)";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtWriteComment.Get2(), NULL));
}

void CClassicDatabase::InsertReference(sqlite3_stmt * stmt, Address_type from, Address_type to)
{
    sqlite3_bind_int64(stmt, 1, AddrToDb(from));
    sqlite3_bind_int64(stmt, 2, AddrToDb(to));
    if (SQLiteStep_Wrapper(stmt) != SQLITE_DONE)
    {
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
    sqlite3_reset(stmt);
}

static orthia::PlatformString_type Escape(const orthia::PlatformString_type & moduleName)
{
    orthia::PlatformString_type copy = moduleName;
    for(orthia::PlatformString_type::iterator it = copy.begin(), it_end = copy.end();
        it != it_end;
        ++it)
    {
        switch(*it)
        {
        case L'\"':
        case L'\'':
        case L'\\':
        case 10:
        case 13:
            *it = L'_';
        }
    }
    return copy;
}

void CClassicDatabase::InsertModule(Address_type baseAddress, 
                             Address_type size, 
                             const orthia::PlatformString_type & moduleName)
{
    CSQLAutoReset autoStatement(m_stmtInsertModule.Get());
    sqlite3_bind_int64(m_stmtInsertModule.Get(), 1, baseAddress);
    sqlite3_bind_int64(m_stmtInsertModule.Get(), 2, size);
    ORTHIA_BIND_PLATFORM_STRING(m_stmtInsertModule.Get(), orthia::Escape(moduleName), 3);

    ORTHIA_CHECK_SQLITE(SQLiteStep_Wrapper(m_stmtInsertModule.Get()), "Can't insert module");
}
void CClassicDatabase::StartSaveModule(Address_type baseAddress,
    Address_type size,
    const orthia::PlatformString_type& moduleName,
    CAutoRollbackClassicDatabase* pRollback,
    bool replaceExisting)
{
    orthia::CAutoCriticalSection guard(m_lock);
    ORTHIA_CHECK_SQLITE2(SQLiteExec_Wrapper(m_pDatabase->Get(), "BEGIN TRANSACTION"));
    pRollback->Init(this);
    if (replaceExisting)
    {
        InsertModule(baseAddress, size, moduleName);
    }
}

void CClassicDatabase::DoneSave()
{
    orthia::CAutoCriticalSection guard(m_lock);

    m_cache.clear();
    ORTHIA_CHECK_SQLITE2(SQLiteExec_Wrapper(m_pDatabase->Get(), "COMMIT TRANSACTION"));
}

CClassicDatabaseBatch::CClassicDatabaseBatch(CClassicDatabase* db)
    : m_db(db), m_committed(false), m_guard(db->GetLock())
{
    m_db->BeginBatchInsert();
}
void CClassicDatabaseBatch::Commit()
{
    m_db->CommitBatchInsert();
    m_committed = true;
}
CClassicDatabaseBatch::~CClassicDatabaseBatch()
{
    if (!m_committed)
        m_db->RollbackTransactionSilent();
}

std::unique_ptr<CClassicDatabaseBatch> CClassicDatabase::BeginBatch()
{
    return std::unique_ptr<CClassicDatabaseBatch>(new CClassicDatabaseBatch(this));
}
void CClassicDatabase::BeginBatchInsert()
{
    ORTHIA_CHECK_SQLITE2(SQLiteExec_Wrapper(m_pDatabase->Get(), "BEGIN TRANSACTION"));
}
void CClassicDatabase::CommitBatchInsert()
{
    ORTHIA_CHECK_SQLITE2(SQLiteExec_Wrapper(m_pDatabase->Get(), "COMMIT TRANSACTION"));
}

void CClassicDatabase::CleanupResources()
{
    orthia::CAutoCriticalSection guard(m_lock);
    m_stmtInsertReferences.SQLReset();
}
void CClassicDatabase::InsertReferencesToInstruction(Address_type offset, const std::vector<CommonReferenceInfo> & references)
{
    orthia::CAutoCriticalSection guard(m_lock);

    for(std::vector<CommonReferenceInfo>::const_iterator it = references.begin(), it_end = references.end();
        it != it_end;
        ++it)
    {
        if (it->external)
        {
            throw std::runtime_error("External references are not supported");
        }
        InsertReference(m_stmtInsertReferences.Get(), it->address, offset); 
    }
}
void CClassicDatabase::InsertReferencesFromInstruction(Address_type offset, const std::vector<CommonReferenceInfo> & references)
{
    orthia::CAutoCriticalSection guard(m_lock);

    for(std::vector<CommonReferenceInfo>::const_iterator it = references.begin(), it_end = references.end();
        it != it_end;
        ++it)
    {
        InsertReference(m_stmtInsertReferences.Get(), offset, it->address);
    }
}
void CClassicDatabase::QueryReferencesToInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtSelectReferencesToRange.Get());
    sqlite3_bind_int64(m_stmtSelectReferencesToRange.Get(), 1, AddrToDb(address1));
    sqlite3_bind_int64(m_stmtSelectReferencesToRange.Get(), 2, AddrToDb(address2));
    pResult->reserve(10);
    pResult->clear();
    for(;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtSelectReferencesToRange.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        else
        if (stepResult == SQLITE_ROW)
        {
            Address_type refTo = DbToAddr(sqlite3_column_int64(m_stmtSelectReferencesToRange.Get(), 0));
            Address_type refFrom = DbToAddr(sqlite3_column_int64(m_stmtSelectReferencesToRange.Get(), 1));

            if (pResult->empty() || pResult->back().address != refTo)
            {
                pResult->push_back(CommonRangeInfo(refTo));
            }
            pResult->back().references.push_back(CommonReferenceInfo(refFrom, false));
            continue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
}
Address_type CClassicDatabase::QueryRouteStart(Address_type offset)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtQueryRouteStart.Get());
    SQLBindInt64(m_stmtQueryRouteStart.Get(), AddrToDb(offset), 1);
    return DbToAddr(SQLite_ReadInt64(m_stmtQueryRouteStart.Get(), true, 0));
}

void CClassicDatabase::QueryReferencesFromInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pReferences)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtSelectReferencesFrom.Get());
    sqlite3_bind_int64(m_stmtSelectReferencesFrom.Get(), 1, AddrToDb(offset));
    pReferences->reserve(10);
    pReferences->clear();
    for(;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtSelectReferencesFrom.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        else
        if (stepResult == SQLITE_ROW)
        {
            Address_type ref = DbToAddr(sqlite3_column_int64(m_stmtSelectReferencesFrom.Get(), 0));
            pReferences->push_back(CommonReferenceInfo(ref, false));
            continue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
}

void CClassicDatabase::QueryReferencesToInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pReferences)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtSelectReferencesTo.Get());
    sqlite3_bind_int64(m_stmtSelectReferencesTo.Get(), 1, AddrToDb(offset));
    pReferences->reserve(10);
    pReferences->clear();
    for(;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtSelectReferencesTo.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        else
        if (stepResult == SQLITE_ROW)
        {
            Address_type ref = DbToAddr(sqlite3_column_int64(m_stmtSelectReferencesTo.Get(), 0));
            pReferences->push_back(CommonReferenceInfo(ref, false));
            continue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
}

void CClassicDatabase::UnloadModule(Address_type address, bool bSilent)
{
    orthia::CAutoCriticalSection guard(m_lock);

    try
    {
        Address_type size = 0;
        std::vector<CommonModuleInfo> allModules;
        QueryModules(&allModules);
        for(std::vector<CommonModuleInfo>::iterator it = allModules.begin(), it_end = allModules.end();
            it != it_end;
            ++it)
        {
            if (it->address == address)
            {
                size = it->size;
                break;
            }
        }
        if (!size)
        {
            if (bSilent)
            {
                return;
            }
            std::stringstream res;
            res<<"Module not found: ";
            std::hex(res);
            res<<address;
            throw std::runtime_error(res.str());
        }
    
        {
            std::stringstream sql;
            sql<<"DELETE FROM tbl_references WHERE ref_address_from BETWEEN "<<AddrToDb(address)<<" AND "<<AddrToDb(address+size);
            std::string sqlString = sql.str();
            ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(), sqlString.c_str()), "Can't unload module");
        }

        {
            std::stringstream sql;
            sql<<"DELETE FROM tbl_modules WHERE mod_address = "<<(long long)address;
            std::string sqlString = sql.str();
            ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(), sqlString.c_str()), "Can't unload module");
        }
    }
    catch(const std::exception & e)
    {
        &e;
        if (!bSilent)
            throw;
    }
}
bool CClassicDatabase::IsModuleExists(Address_type address)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtSelectModule.Get());
    sqlite3_bind_int64(m_stmtSelectModule.Get(), 1, address);
    int stepResult = SQLiteStep_Wrapper(m_stmtSelectModule.Get());
    if (stepResult == SQLITE_ROW)
    {
        return true;
    }
    return false;
}
void CClassicDatabase::QueryReferencesFromInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtSelectReferencesFromRange.Get());
    sqlite3_bind_int64(m_stmtSelectReferencesFromRange.Get(), 1, AddrToDb(address1));
    sqlite3_bind_int64(m_stmtSelectReferencesFromRange.Get(), 2, AddrToDb(address2));
    pResult->reserve(10);
    pResult->clear();
    for(;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtSelectReferencesFromRange.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        else
        if (stepResult == SQLITE_ROW)
        {
            Address_type refFrom = DbToAddr(sqlite3_column_int64(m_stmtSelectReferencesFromRange.Get(), 0));
            Address_type refTo = DbToAddr(sqlite3_column_int64(m_stmtSelectReferencesFromRange.Get(), 1));
            if (pResult->empty() || pResult->back().address != refFrom)
            {
                pResult->push_back(CommonRangeInfo(refFrom));
            }
            pResult->back().references.push_back(CommonReferenceInfo(refTo, false));
            continue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
}
bool CClassicDatabase::QueryModule(Address_type moduleAddress, CommonModuleInfo* pResult)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtQueryModuleById.Get());
    sqlite3_bind_int64(m_stmtQueryModuleById.Get(), 1, moduleAddress);

    for (;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtQueryModuleById.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        else
            if (stepResult == SQLITE_ROW)
            {
                Address_type size = sqlite3_column_int64(m_stmtQueryModuleById.Get(), 0);
                std::string name = (char*)sqlite3_column_text(m_stmtQueryModuleById.Get(), 1);
                *pResult = CommonModuleInfo(moduleAddress, size, orthia::Utf8ToPlatformString(name));
                return true;
            }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
    return false;
}
bool CClassicDatabase::QueryNearestModule(Address_type address, CommonModuleInfo* pResult)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtQueryNearestModuleById.Get());
    sqlite3_bind_int64(m_stmtQueryNearestModuleById.Get(), 1, address);

    for (;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtQueryNearestModuleById.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        else
            if (stepResult == SQLITE_ROW)
            {
                Address_type moduleAddress = sqlite3_column_int64(m_stmtQueryNearestModuleById.Get(), 0);
                Address_type size = sqlite3_column_int64(m_stmtQueryNearestModuleById.Get(), 1);
                std::string name = (char*)sqlite3_column_text(m_stmtQueryNearestModuleById.Get(), 2);
                *pResult = CommonModuleInfo(moduleAddress, size, orthia::Utf8ToPlatformString(name));
                return true;
            }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
    return false;
}
void CClassicDatabase::QueryModules(std::vector<CommonModuleInfo> * pResult)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtQueryModules.Get());
    pResult->reserve(10);
    pResult->clear();
    for(;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtQueryModules.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        else
        if (stepResult == SQLITE_ROW)
        {
            Address_type address = sqlite3_column_int64(m_stmtQueryModules.Get(), 0);
            Address_type size = sqlite3_column_int64(m_stmtQueryModules.Get(), 1);
            std::string name = (char*)sqlite3_column_text(m_stmtQueryModules.Get(), 2);
            pResult->push_back(CommonModuleInfo(address, size, orthia::Utf8ToPlatformString(name)));
            continue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
}
void CClassicDatabase::RollbackTransactionSilent()
{
    orthia::CAutoCriticalSection guard(m_lock);

    m_cache.clear();
    SQLiteExec_Wrapper(m_pDatabase->Get(), "ROLLBACK TRANSACTION");
}

void CClassicDatabase::InsertMetaInfo(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtInsertMetainfo.Get());
    sqlite3_bind_int64(m_stmtInsertMetainfo.Get(), 1, moduleAddress);
    if (metaAddress != DI_MAX_OPERAND_SIZE)
    {
        sqlite3_bind_int64(m_stmtInsertMetainfo.Get(), 2, AddrToDb(metaAddress));
    }
    sqlite3_bind_int64(m_stmtInsertMetainfo.Get(), 3, metaType);
    SQLBindUtf8String(m_stmtInsertMetainfo.Get(), text, 4);

    ORTHIA_CHECK_SQLITE(SQLiteStep_Wrapper(m_stmtInsertMetainfo.Get()), "Can't insert meta info");
}
void CClassicDatabase::QueryMetaInfo(int metaType, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtSelectMetainfo_All.Get());
    sqlite3_bind_int64(m_stmtSelectMetainfo_All.Get(), 1, metaType);
    for (;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtSelectMetainfo_All.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        if (stepResult != SQLITE_ROW)
        {
            throw std::runtime_error("SQLiteStep_Wrapper failed");
        }

        // meta_mod_id, meta_address, meta_type, meta_info
        Address_type moduleAddress = sqlite3_column_int64(m_stmtSelectMetainfo_All.Get(), 0);
        Address_type metaAddress = DbToAddr(sqlite3_column_int64(m_stmtSelectMetainfo_All.Get(), 1));
        Address_type type = sqlite3_column_int64(m_stmtSelectMetainfo_All.Get(), 2);
        auto text = SQLReadUtf8String(m_stmtSelectMetainfo_All.Get(), 3);
        if (!handler(moduleAddress, (int)type, text, metaAddress))
        {
            break;
        }
    }
}

void CClassicDatabase::QueryMetaInfoModule2(Address_type moduleAddress, int metaType1, int metaType2, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler, Address_type addressHint)
{
    orthia::CAutoCriticalSection guard(m_lock);

    sqlite3_stmt* stmt = addressHint ? m_stmtSelectMetainfo_Module2_FromAddress.Get() : m_stmtSelectMetainfo_Module2.Get();
    CSQLAutoReset autoStatement(stmt);
    sqlite3_bind_int64(stmt, 1, moduleAddress);
    sqlite3_bind_int64(stmt, 2, metaType1);
    sqlite3_bind_int64(stmt, 3, metaType2);
    if (addressHint)
    {
        sqlite3_bind_int64(stmt, 4, AddrToDb(addressHint));
    }
    for (;;)
    {
        int stepResult = SQLiteStep_Wrapper(stmt);
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        if (stepResult != SQLITE_ROW)
        {
            throw std::runtime_error("SQLiteStep_Wrapper failed");
        }

        Address_type metaAddress = DbToAddr(sqlite3_column_int64(stmt, 0));
        Address_type type = sqlite3_column_int64(stmt, 1);
        auto text = SQLReadUtf8String(stmt, 2);
        if (!handler(moduleAddress, (int)type, text, metaAddress))
        {
            break;
        }
    }
}

void CClassicDatabase::QueryMetaInfoByAddress(int metaType, Address_type metaAddress, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtSelectMetainfo_Address.Get());
    sqlite3_bind_int64(m_stmtSelectMetainfo_Address.Get(), 1, metaType);
    sqlite3_bind_int64(m_stmtSelectMetainfo_Address.Get(), 2, AddrToDb(metaAddress));
    for (;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtSelectMetainfo_Address.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        if (stepResult != SQLITE_ROW)
        {
            throw std::runtime_error("SQLiteStep_Wrapper failed");
        }

        // meta_mod_id, meta_address, meta_type, meta_info
        Address_type moduleAddress = sqlite3_column_int64(m_stmtSelectMetainfo_Address.Get(), 0);
        Address_type type = sqlite3_column_int64(m_stmtSelectMetainfo_Address.Get(), 1);
        auto text = SQLReadUtf8String(m_stmtSelectMetainfo_Address.Get(), 2);
        if (!handler(moduleAddress, (int)type, text, metaAddress))
        {
            break;
        }
    }
}

void CClassicDatabase::QueryMetaInfoByNearestAddress(int metaType, Address_type address, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler, int metaType2)
{
    orthia::CAutoCriticalSection guard(m_lock);

    sqlite3_stmt* stmt = (metaType2 != -1) ? m_stmtSelectMetainfo_NearestAddress2.Get() : m_stmtSelectMetainfo_NearestAddress.Get();
    CSQLAutoReset autoStatement(stmt);
    sqlite3_bind_int64(stmt, 1, metaType);
    sqlite3_bind_int64(stmt, 2, AddrToDb(address));
    if (metaType2 != -1)
    {
        sqlite3_bind_int64(stmt, 3, metaType2);
    }
    for (;;)
    {
        int stepResult = SQLiteStep_Wrapper(stmt);
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        if (stepResult != SQLITE_ROW)
        {
            throw std::runtime_error("SQLiteStep_Wrapper failed");
        }

        // meta_mod_id, meta_type, meta_info, meta_address
        Address_type moduleAddress = sqlite3_column_int64(stmt, 0);
        Address_type type = sqlite3_column_int64(stmt, 1);
        auto text = SQLReadUtf8String(stmt, 2);
        Address_type metaAddress = DbToAddr(sqlite3_column_int64(stmt, 3));
        if (!handler(moduleAddress, (int)type, text, metaAddress))
        {
            break;
        }
    }
}

void CClassicDatabase::QueryMetaInfoByAddressRange(int metaType, Address_type addr1, Address_type addr2, std::function<bool(Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)> handler)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtSelectMetainfo_AddressRange.Get());
    sqlite3_bind_int64(m_stmtSelectMetainfo_AddressRange.Get(), 1, metaType);
    sqlite3_bind_int64(m_stmtSelectMetainfo_AddressRange.Get(), 2, AddrToDb(addr1));
    sqlite3_bind_int64(m_stmtSelectMetainfo_AddressRange.Get(), 3, AddrToDb(addr2));
    for (;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtSelectMetainfo_AddressRange.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        if (stepResult != SQLITE_ROW)
        {
            throw std::runtime_error("SQLiteStep_Wrapper failed");
        }

        // meta_mod_id, meta_type, meta_info, meta_address
        Address_type moduleAddress = sqlite3_column_int64(m_stmtSelectMetainfo_AddressRange.Get(), 0);
        Address_type type = sqlite3_column_int64(m_stmtSelectMetainfo_AddressRange.Get(), 1);
        auto text = SQLReadUtf8String(m_stmtSelectMetainfo_AddressRange.Get(), 2);
        Address_type metaAddress = DbToAddr(sqlite3_column_int64(m_stmtSelectMetainfo_AddressRange.Get(), 3));
        if (!handler(moduleAddress, (int)type, text, metaAddress))
        {
            break;
        }
    }
}

int CClassicDatabase::QueryMetaInfoModule2_Count(Address_type moduleAddress, int metaType1, int metaType2)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtSelectMetainfo_Module2.Get());
    sqlite3_bind_int64(m_stmtSelectMetainfo_Module2.Get(), 1, moduleAddress);
    sqlite3_bind_int64(m_stmtSelectMetainfo_Module2.Get(), 2, metaType1);
    sqlite3_bind_int64(m_stmtSelectMetainfo_Module2.Get(), 3, metaType2);
    for (;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtSelectMetainfo_Module2.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        if (stepResult != SQLITE_ROW)
        {
            throw std::runtime_error("SQLiteStep_Wrapper failed");
        }

        // meta_mod_id, meta_address, meta_type, meta_info
        return sqlite3_column_int(m_stmtSelectMetainfo_Module2.Get(), 0);
    }
    return 0;
}


void CClassicDatabase::QueryAllComments(std::function<bool(Address_type address, const std::string& text)> handler)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtSelectAllComments.Get());
    for (;;)
    {
        int stepResult = SQLiteStep_Wrapper(m_stmtSelectAllComments.Get());
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        if (stepResult != SQLITE_ROW)
        {
            throw std::runtime_error("SQLiteStep_Wrapper failed");
        }

        // meta_mod_id, meta_address, meta_type, meta_info
        Address_type address = sqlite3_column_int64(m_stmtSelectAllComments.Get(), 0);
        auto text = SQLReadUtf8String(m_stmtSelectAllComments.Get(), 1);
        if (!handler(address, text))
        {
            break;
        }
    }
}
void CClassicDatabase::InsertComment(Address_type address, const std::string& text)
{
    orthia::CAutoCriticalSection guard(m_lock);

    CSQLAutoReset autoStatement(m_stmtWriteComment.Get());
    sqlite3_bind_int64(m_stmtWriteComment.Get(), 1, address);
    SQLBindUtf8String(m_stmtWriteComment.Get(), text, 2);

    ORTHIA_CHECK_SQLITE(SQLiteStep_Wrapper(m_stmtWriteComment.Get()), "Can't insert meta info");
}

}