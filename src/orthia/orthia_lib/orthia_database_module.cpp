#include "orthia_database_module.h"

namespace orthia
{

void sql_less_or_equal( sqlite3_context* ctx, int argc, sqlite3_value** argv )
{
    if (argc !=2 )
    {    
        sqlite3_result_null(ctx);
        return;
    }
    Address_type value1 = (Address_type)( sqlite3_value_int64( argv[ 0 ] ) );
    Address_type value2 = (Address_type)( sqlite3_value_int64( argv[ 1 ] ) );
    sqlite3_result_int( ctx, value1 <= value2 );
}

// CDBVersion
CDBVersion::CDBVersion()
{
}
void CDBVersion::Init(sqlite3 * database)
{
    char * buffer = 0;
    buffer = "SELECT MAX(ver_database_version) FROM tbl_version";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(database, buffer, (int)strlen(buffer), m_getDBVersion.Get2(), NULL));
    
    buffer = "INSERT INTO tbl_version (ver_database_version) VALUES (?1)";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(database, buffer, (int)strlen(buffer), m_addDBVersion.Get2(), NULL));
}

int CDBVersion::GetDBVersion()
{
    orthia::CSQLAutoReset autoStatement(m_getDBVersion.Get());

    orthia::SQLResult_1_Int result;
    orthia::SQLite_ReadCustom(m_getDBVersion.Get(), true, orthia::SQLResult_1_Int(0), &result, false, true);
    return result.m_value;
}
void CDBVersion::AddDBVersion(int newVersion)
{
    orthia::CSQLAutoReset autoStatement(m_addDBVersion.Get());
    orthia::SQLBindInt(m_addDBVersion.Get(), newVersion, 1);
    orthia::SQLExecute(m_addDBVersion.Get());
}
//--------
CDatabase::CDatabase()
{
}
CDatabase::~CDatabase()
{

}
void CDatabase::Init()
{
    ORTHIA_CHECK_SQLITE2(sqlite3_create_function(m_pDatabase->Get(), "UINT_LESSOE", 2, SQLITE_ANY, NULL, sql_less_or_equal, NULL, NULL ));

    DoVersionScripts();

    m_pClassicDatabase = new CClassicDatabase(m_pDatabase);
    m_pVMDatabase = new CVMDatabase(m_pDatabase);
}

void CDatabase::DoUpdate_0_1()
{
    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(),"CREATE TABLE IF NOT EXISTS tbl_vm_vms (vms_id INTEGER PRIMARY KEY, vms_name TEXT, vms_creation_time DATETIME DEFAULT CURRENT_TIMESTAMP, vms_last_write_time DATETIME DEFAULT CURRENT_TIMESTAMP)"), 
        "Can't update the database");

    // vmods_flags: executable, file, disabled, exec-results
    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(),"CREATE TABLE IF NOT EXISTS tbl_vm_modules(vmods_vm_id INTEGER, vmods_flags INTEGER DEFAULT 0, vmods_pos_in_vm INTEGER, vmods_info_xml TEXT, vmods_raw_data BLOB, FOREIGN KEY(vmods_vm_id) REFERENCES tbl_vm_vms(vms_id))"), 
        "Can't update the database");

}
void CDatabase::DoUpdate_1_2()
{
    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(), "CREATE TABLE IF NOT EXISTS tbl_metainfo(meta_mod_id INTEGER, meta_address INTEGER, meta_type INTEGER, meta_info TEXT, FOREIGN KEY(meta_mod_id) REFERENCES tbl_modules(mod_address) ON DELETE CASCADE)"),
        "Can't create database");

    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(), "CREATE TABLE IF NOT EXISTS tbl_comments(com_address INTEGER PRIMARY KEY, com_text TEXT)"),
        "Can't create database");
}
void CDatabase::DoUpdate_2_3()
{
    // XOR all address columns with 0x8000000000000000 so unsigned order maps to signed order,
    // enabling native <= / >= comparisons and index use on tbl_references.
    // SQLite has no ^ operator, so XOR is expressed as (a & ~b) | (~a & b).
    // The constant (INT64_MIN) is bound via sqlite3_bind_int64 to avoid literal overflow issues.
    const char * sql =
        "UPDATE tbl_references SET "
        "ref_address_from = (ref_address_from & ~?1) | (~ref_address_from & ?1), "
        "ref_address_to   = (ref_address_to   & ~?1) | (~ref_address_to   & ?1)";
    orthia::CSQLStatement stmt;
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), sql, -1, stmt.Get2(), NULL));
    ORTHIA_CHECK_SQLITE2(sqlite3_bind_int64(stmt.Get(), 1, (sqlite3_int64)0x8000000000000000ULL));
    if (SQLiteStep_Wrapper(stmt.Get()) != SQLITE_DONE)
    {
        throw std::runtime_error("Can't migrate tbl_references to XOR encoding");
    }

    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(),
        "CREATE INDEX IF NOT EXISTS idx_ref_to ON tbl_references(ref_address_to)"),
        "Can't create index on tbl_references");
}
void CDatabase::DoVersionScripts()
{
    CSQLTransaction transaction(m_pDatabase->Get());

    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(),"CREATE TABLE IF NOT EXISTS tbl_version (ver_database_version INTEGER PRIMARY KEY, ver_action_time DATETIME DEFAULT CURRENT_TIMESTAMP)"), 
        "Can't create database");

    m_dbVersion.Init(m_pDatabase->Get());

    int version = m_dbVersion.GetDBVersion();

#define ORTHIA_CURRENT_DB_VERSION_INT       3
    if (version != ORTHIA_CURRENT_DB_VERSION_INT)
    {
        switch(version)
        {
        default: throw std::runtime_error("Unknown database version: " + orthia::ObjectToString_Ansi(version));
        case 0:
            DoUpdate_0_1();
            m_dbVersion.AddDBVersion(1);
        case 1:
            DoUpdate_1_2();
            m_dbVersion.AddDBVersion(2);
        case 2:
            DoUpdate_2_3();
            m_dbVersion.AddDBVersion(3);
        case ORTHIA_CURRENT_DB_VERSION_INT:
            ;
        }
    }

    transaction.Commit();
}
void CDatabase::OpenExisting(const orthia::PlatformString_type & fullFileName)
{
    CSQLDatabase database;
    ORTHIA_CHECK_SQLITE(ORTHIA_SQLITE3_OPEN(fullFileName.c_str(), database.Get2()), "Can't open the database: "<<orthia::ToAnsiString_Silent(fullFileName));
    m_pDatabase = new CSQLDatabase2(database);
    Init();
}
void CDatabase::CreateNew(const orthia::PlatformString_type & fullFileName)
{
    PlatformDeleteFile(fullFileName);

    CSQLDatabase database;
    ORTHIA_CHECK_SQLITE(ORTHIA_SQLITE3_OPEN(fullFileName.c_str(), database.Get2()), "Can't create the database: "<<orthia::ToAnsiString_Silent(fullFileName));
    m_pDatabase = new CSQLDatabase2(database);

    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(), "PRAGMA encoding = \"UTF-8\""), "Can't create database");

    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(), "PRAGMA foreign_keys=ON"), "Can't create database");

    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(),"CREATE TABLE IF NOT EXISTS tbl_references (ref_address_from INTEGER, ref_address_to INTEGER)"), 
        "Can't create database");

    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_pDatabase->Get(),"CREATE TABLE IF NOT EXISTS tbl_modules (mod_address INTEGER PRIMARY KEY, mod_size INTEGER, mod_name TEXT)"), 
        "Can't create database");

    Init();
}

orthia::intrusive_ptr<CClassicDatabase> CDatabase::GetClassicDatabase()
{
    return m_pClassicDatabase;
}
orthia::intrusive_ptr<CVMDatabase> CDatabase::GetVMDatabase()
{
    return m_pVMDatabase;
}
// CDatabaseManager
CDatabaseManager::CDatabaseManager()
{
}
CDatabaseManager::~CDatabaseManager()
{
}
 
// file api
void CDatabaseManager::CreateNew(const orthia::PlatformString_type & fullFileName)
{
    orthia::intrusive_ptr<CDatabase> database(new CDatabase());
    database->CreateNew(fullFileName);

    CAutoCriticalSection guard(m_createLock);
    m_database = database;
}
void CDatabaseManager::OpenExisting(const orthia::PlatformString_type & fullFileName)
{
    orthia::intrusive_ptr<CDatabase> database(new CDatabase());
    database->OpenExisting(fullFileName);

    CAutoCriticalSection guard(m_createLock);
    m_database = database;
}

orthia::intrusive_ptr<CDatabase> CDatabaseManager::GetDatabase()
{
    CAutoCriticalSection guard(m_createLock);

    if (!m_database)
        throw std::runtime_error("No database initialized");
    return m_database;
}
orthia::intrusive_ptr<CClassicDatabase> CDatabaseManager::GetClassicDatabase()
{
    return GetDatabase()->GetClassicDatabase();
}
orthia::intrusive_ptr<CVMDatabase> CDatabaseManager::GetVMDatabase()
{
    return GetDatabase()->GetVMDatabase();
}

}