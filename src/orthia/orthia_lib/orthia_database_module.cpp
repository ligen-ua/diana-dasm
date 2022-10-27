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
    ORTHIA_CHECK_SQLITE(sqlite3_exec(m_pDatabase->Get(),"CREATE TABLE IF NOT EXISTS tbl_vm_vms (vms_id INTEGER PRIMARY KEY, vms_name TEXT, vms_creation_time DATETIME DEFAULT CURRENT_TIMESTAMP, vms_last_write_time DATETIME DEFAULT CURRENT_TIMESTAMP)",
                        0,0,0), "Can't update the database");

    // vmods_flags: executable, file, disabled, exec-results
    ORTHIA_CHECK_SQLITE(sqlite3_exec(m_pDatabase->Get(),"CREATE TABLE IF NOT EXISTS tbl_vm_modules(vmods_vm_id INTEGER, vmods_flags INTEGER DEFAULT 0, vmods_pos_in_vm INTEGER, vmods_info_xml TEXT, vmods_raw_data BLOB, FOREIGN KEY(vmods_vm_id) REFERENCES tbl_vm_vms(vms_id))",
                        0,0,0), "Can't update the database");

}
void CDatabase::DoVersionScripts()
{
    CSQLTransaction transaction(m_pDatabase->Get());

    ORTHIA_CHECK_SQLITE(sqlite3_exec(m_pDatabase->Get(),"CREATE TABLE IF NOT EXISTS tbl_version (ver_database_version INTEGER PRIMARY KEY, ver_action_time DATETIME DEFAULT CURRENT_TIMESTAMP)",
                        0,0,0), "Can't create database");

    m_dbVersion.Init(m_pDatabase->Get());

    int version = m_dbVersion.GetDBVersion();

#define ORTHIA_CURRENT_DB_VERSION_INT       1
    if (version != ORTHIA_CURRENT_DB_VERSION_INT)
    {
        switch(version)
        {
        default: throw std::runtime_error("Unknown database version: " + orthia::ObjectToString_Ansi(version));
        case 0:
            DoUpdate_0_1();
            m_dbVersion.AddDBVersion(1);
        case ORTHIA_CURRENT_DB_VERSION_INT:
            ;
        }
    }

    transaction.Commit();
}
void CDatabase::OpenExisting(const std::wstring & fullFileName)
{
    CSQLDatabase database;
    ORTHIA_CHECK_SQLITE(sqlite3_open16(fullFileName.c_str(), database.Get2()), "Can't open the database: "<<orthia::ToAnsiString_Silent(fullFileName));
    m_pDatabase = new CSQLDatabase2(database);
    Init();
}
void CDatabase::CreateNew(const std::wstring & fullFileName)
{
    BOOL res = DeleteFile(fullFileName.c_str());
    if (!res)
    {
        ULONG error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND)
        {
            ORTHIA_THROW_WIN32("Can't access the database: "<<orthia::ToAnsiString_Silent(fullFileName)<<", code: "<<orthia____code);
        }
    }
    CSQLDatabase database;
    ORTHIA_CHECK_SQLITE(sqlite3_open16(fullFileName.c_str(), database.Get2()), "Can't create the database: "<<orthia::ToAnsiString_Silent(fullFileName));
    m_pDatabase = new CSQLDatabase2(database);

    ORTHIA_CHECK_SQLITE(sqlite3_exec(m_pDatabase->Get(),"CREATE TABLE IF NOT EXISTS tbl_references (ref_address_from INTEGER, ref_address_to INTEGER)",
                        0,0,0), "Can't create database");

    ORTHIA_CHECK_SQLITE(sqlite3_exec(m_pDatabase->Get(),"CREATE TABLE IF NOT EXISTS tbl_modules (mod_address INTEGER, mod_size INTEGER, mod_name TEXT)",
                        0,0,0), "Can't create database");

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
void CDatabaseManager::CreateNew(const std::wstring & fullFileName)
{
    orthia::intrusive_ptr<CDatabase> database(new CDatabase());
    database->CreateNew(fullFileName);
    m_database = database;
}
void CDatabaseManager::OpenExisting(const std::wstring & fullFileName)
{
    orthia::intrusive_ptr<CDatabase> database(new CDatabase());
    database->OpenExisting(fullFileName);
    m_database = database;
}

orthia::intrusive_ptr<CDatabase> CDatabaseManager::GetDatabase()
{
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