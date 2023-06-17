#include "orthia_database_impl_classic.h"

namespace orthia
{

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
    buffer = "SELECT * FROM tbl_modules ORDER BY mod_address";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtQueryModules.Get2(), NULL));
    buffer = "SELECT ref_address_to, ref_address_from FROM tbl_references WHERE UINT_LESSOE(?1, ref_address_to) AND UINT_LESSOE(ref_address_to, ?2) ORDER BY ref_address_to, ref_address_from";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectReferencesToRange.Get2(), NULL));
    buffer = "SELECT ref_address_to FROM tbl_references WHERE ref_address_from = ?1 ORDER BY ref_address_to";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectReferencesFrom.Get2(), NULL));
    
    buffer = "SELECT ref_address_from, ref_address_to FROM tbl_references WHERE UINT_LESSOE(?1, ref_address_from) AND UINT_LESSOE(ref_address_from, ?2) ORDER BY ref_address_from, ref_address_to";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectReferencesFromRange.Get2(), NULL));

    buffer = "SELECT ref_address_to FROM tbl_references WHERE UINT_LESSOE(ref_address_to, ?1) ORDER BY ref_address_to DESC LIMIT 1";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtQueryRouteStart.Get2(), NULL));
}

void CClassicDatabase::InsertReference(sqlite3_stmt * stmt, Address_type from, Address_type to)
{
    sqlite3_bind_int64(stmt, 1, from);
    sqlite3_bind_int64(stmt, 2, to);
    if (SQLiteStep_Wrapper(stmt) != SQLITE_DONE)
    {
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
    sqlite3_reset(stmt);
}

static std::wstring Escape(const std::wstring & moduleName)
{
    std::wstring copy = moduleName;
    for(std::wstring::iterator it = copy.begin(), it_end = copy.end();
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
                             const std::wstring & moduleName)
{
    std::stringstream sql;
    sql<<"INSERT INTO tbl_modules VALUES("<<(long long)baseAddress<<","<<size<<",\""<<orthia::ToAnsiString_Silent(orthia::Escape(moduleName))<<"\")";
    std::string sqlString = sql.str();
    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(),sqlString.c_str()), L"Can't insert module");
}
void CClassicDatabase::StartSaveModule(Address_type baseAddress, 
                                Address_type size, 
                                const std::wstring & moduleName,
                                CAutoRollbackClassicDatabase * pRollback)
{
    ORTHIA_CHECK_SQLITE2(SQLiteExec_Wrapper(m_pDatabase->Get(), "BEGIN TRANSACTION"));
    pRollback->Init(this);
    char * buffer = 0;
    buffer = "INSERT INTO tbl_references VALUES(?1, ?2)";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtInsertReferences.Get2(), NULL));
    InsertModule(baseAddress, size, moduleName);
}
void CClassicDatabase::DoneSave()
{
    m_cache.clear();
    ORTHIA_CHECK_SQLITE2(SQLiteExec_Wrapper(m_pDatabase->Get(), "COMMIT TRANSACTION"));
}
void CClassicDatabase::CleanupResources()
{
    m_stmtInsertReferences.Finalize();    
}
void CClassicDatabase::InsertReferencesToInstruction(Address_type offset, const std::vector<CommonReferenceInfo> & references)
{
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
    for(std::vector<CommonReferenceInfo>::const_iterator it = references.begin(), it_end = references.end();
        it != it_end;
        ++it)
    {
        InsertReference(m_stmtInsertReferences.Get(), offset, it->address);
    }
}
void CClassicDatabase::QueryReferencesToInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult)
{
    CSQLAutoReset autoStatement(m_stmtSelectReferencesToRange.Get());
    sqlite3_bind_int64(m_stmtSelectReferencesToRange.Get(), 1, address1);
    sqlite3_bind_int64(m_stmtSelectReferencesToRange.Get(), 2, address2);
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
            Address_type refTo = sqlite3_column_int64(m_stmtSelectReferencesToRange.Get(), 0);
            Address_type refFrom = sqlite3_column_int64(m_stmtSelectReferencesToRange.Get(), 1);

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
    CSQLAutoReset autoStatement(m_stmtQueryRouteStart.Get());
    SQLBindInt64(m_stmtQueryRouteStart.Get(), offset, 1);
    return SQLite_ReadInt64(m_stmtQueryRouteStart.Get(), true, 0);
}

void CClassicDatabase::QueryReferencesFromInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pReferences)
{
    CSQLAutoReset autoStatement(m_stmtSelectReferencesFrom.Get());
    sqlite3_bind_int64(m_stmtSelectReferencesFrom.Get(), 1, offset);
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
            Address_type ref = sqlite3_column_int64(m_stmtSelectReferencesFrom.Get(), 0);
            pReferences->push_back(CommonReferenceInfo(ref, false));
            continue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
}

void CClassicDatabase::QueryReferencesToInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pReferences)
{
    CSQLAutoReset autoStatement(m_stmtSelectReferencesTo.Get());
    sqlite3_bind_int64(m_stmtSelectReferencesTo.Get(), 1, offset);
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
            Address_type ref = sqlite3_column_int64(m_stmtSelectReferencesTo.Get(), 0);
            pReferences->push_back(CommonReferenceInfo(ref, false));
            continue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
}

void CClassicDatabase::UnloadModule(Address_type address, bool bSilent)
{
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
            sql<<"DELETE FROM tbl_references WHERE UINT_LESSOE("<<(long long)address<<", ref_address_from) and UINT_LESSOE(ref_address_from, "<<(long long)(address+size)<<")";
            std::string sqlString = sql.str();
            ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(), sqlString.c_str()), L"Can't unload module");
        }

        {
            std::stringstream sql;
            sql<<"DELETE FROM tbl_modules WHERE mod_address = "<<(long long)address;
            std::string sqlString = sql.str();
            ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(), sqlString.c_str()), L"Can't unload module");
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
    CSQLAutoReset autoStatement(m_stmtSelectReferencesFromRange.Get());
    sqlite3_bind_int64(m_stmtSelectReferencesFromRange.Get(), 1, address1);
    sqlite3_bind_int64(m_stmtSelectReferencesFromRange.Get(), 2, address2);
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
            Address_type refFrom = sqlite3_column_int64(m_stmtSelectReferencesFromRange.Get(), 0);
            Address_type refTo = sqlite3_column_int64(m_stmtSelectReferencesFromRange.Get(), 1);
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

void CClassicDatabase::QueryModules(std::vector<CommonModuleInfo> * pResult)
{
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
            pResult->push_back(CommonModuleInfo(address, size, orthia::ToWideString(name)));
            continue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
}
void CClassicDatabase::RollbackTransactionSilent()
{
    m_cache.clear();
    SQLiteExec_Wrapper(m_pDatabase->Get(), "ROLLBACK TRANSACTION");
}

}