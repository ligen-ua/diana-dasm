#include "orthia_database_impl_vm.h"


namespace orthia
{

CVMDatabase::CVMDatabase(intrusive_ptr<CSQLDatabase2> pDatabase)
    :
        m_pDatabase(pDatabase)
{
    Init();
}
CVMDatabase::~CVMDatabase()
{

}

void CVMDatabase::Init()
{
    char * buffer = 0;
    buffer = "SELECT last_insert_rowid()";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectRowId.Get2(), NULL));

    // vms
    buffer = "INSERT INTO tbl_vm_vms (vms_name) VALUES(?1)";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtInsertVM.Get2(), NULL));

    buffer = "INSERT INTO tbl_vm_vms (vms_name, vms_id) VALUES(?1, ?2)";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtInsertVM_withID.Get2(), NULL));

    buffer = "DELETE FROM tbl_vm_vms WHERE vms_id = ?1";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtDeleteVM.Get2(), NULL));

    buffer = "SELECT vms_id, vms_name, vms_creation_time, vms_last_write_time FROM tbl_vm_vms ORDER BY vms_id";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectVMs.Get2(), NULL));
    
    buffer = "SELECT vms_id, vms_name, vms_creation_time, vms_last_write_time FROM tbl_vm_vms WHERE vms_id = ?1";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectVMByID.Get2(), NULL));

    buffer = "SELECT vms_id, vms_name, vms_creation_time, vms_last_write_time FROM tbl_vm_vms WHERE vms_name = ?1 ORDER BY vms_id";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectVMByName.Get2(), NULL));

    buffer = "UPDATE tbl_vm_vms SET vms_last_write_time = CURRENT_TIMESTAMP WHERE vms_id = ?1";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtUpdateVMTimeByID.Get2(), NULL));
    // modules
    buffer = "SELECT MAX(vmods_pos_in_vm) FROM tbl_vm_modules WHERE vmods_vm_id = ?1";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectMaxModulePosByVM.Get2(), NULL));

    buffer = "INSERT INTO tbl_vm_modules (vmods_vm_id, vmods_pos_in_vm) VALUES(?1, ?2)";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtInsertModule.Get2(), NULL));

    buffer = "DELETE FROM tbl_vm_modules WHERE vmods_vm_id = ?1 AND vmods_pos_in_vm = ?2";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtDeleteModule.Get2(), NULL));

    buffer = "DELETE FROM tbl_vm_modules WHERE vmods_vm_id = ?1";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtDeleteAllModulesOfVM.Get2(), NULL));

    buffer = "UPDATE tbl_vm_modules SET vmods_info_xml = ?3, vmods_raw_data = ?4 WHERE vmods_vm_id = ?1 AND vmods_pos_in_vm = ?2";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtUpdateModule.Get2(), NULL));

    buffer = "UPDATE tbl_vm_modules SET vmods_info_xml = ?3 WHERE vmods_vm_id = ?1 AND vmods_pos_in_vm = ?2";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtUpdateModuleXML.Get2(), NULL));

    buffer = "UPDATE tbl_vm_modules SET vmods_flags = ((vmods_flags & ?3) | ?4) WHERE vmods_vm_id = ?1 AND vmods_pos_in_vm = ?2";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtUpdateModuleFlags.Get2(), NULL));
    // -- 
    buffer = "SELECT vmods_pos_in_vm, vmods_info_xml, vmods_flags, vmods_raw_data FROM tbl_vm_modules WHERE vmods_vm_id = ?1 ORDER BY vmods_pos_in_vm";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectModulesByVM.Get2(), NULL));
    
    buffer = "SELECT vmods_pos_in_vm, vmods_info_xml, vmods_flags, vmods_raw_data FROM tbl_vm_modules WHERE vmods_vm_id = ?1 AND vmods_pos_in_vm = ?";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectModuleByID.Get2(), NULL));

    buffer = "SELECT vmods_pos_in_vm, vmods_info_xml, vmods_flags FROM tbl_vm_modules WHERE vmods_vm_id = ?1 ORDER BY vmods_pos_in_vm";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectModulesByVM_Nodata.Get2(), NULL));
    
    buffer = "SELECT vmods_pos_in_vm, vmods_info_xml, vmods_flags FROM tbl_vm_modules WHERE vmods_vm_id = ?1 AND vmods_pos_in_vm = ?";
    ORTHIA_CHECK_SQLITE2(sqlite3_prepare_v2(m_pDatabase->Get(), buffer, (int)strlen(buffer), m_stmtSelectModuleByID_Nodata.Get2(), NULL));
}

long long CVMDatabase::QueryLastId()
{
    sqlite3_stmt * statement = m_stmtSelectRowId.Get();
    CSQLAutoReset autoStatement(statement);
    return SQLite_ReadInt(m_stmtSelectRowId.Get(), false, 0);
}

long long CVMDatabase::AddNewVM(const std::wstring & name,
                                const long long * pID)
{
    CSQLTransaction transaction;
    transaction.Init(m_pDatabase->Get());
    {
        CSQLAutoReset autoStatement(m_stmtSelectVMByName.Get());
        SQLBindWideString(m_stmtSelectVMByName.Get(), name, 1);
        if (SQLStep(m_stmtSelectVMByName.Get()))
        {
            throw std::runtime_error("Can't add new VM, name already exists");
        }
    }
    long long id = 0;
    if (pID)
    {
        CSQLAutoReset autoStatement(m_stmtInsertVM_withID.Get());
        SQLBindWideString(m_stmtInsertVM_withID.Get(), name, 1);
        SQLBindInt64(m_stmtInsertVM_withID.Get(), *pID, 2);
        SQLExecute(m_stmtInsertVM_withID.Get());
        id = *pID;
    }
    else
    {
        CSQLAutoReset autoStatement(m_stmtInsertVM.Get());
        SQLBindWideString(m_stmtInsertVM.Get(), name, 1);
        SQLExecute(m_stmtInsertVM.Get());
        id = QueryLastId();
    }
    transaction.Commit();
    return id;
}
void CVMDatabase::DelVM(long long id)
{
    CSQLTransaction transaction;
    {
        CSQLAutoReset autoStatement(m_stmtDeleteVM.Get());
        SQLBindInt64(m_stmtDeleteVM.Get(), id, 1);
        SQLExecute(m_stmtDeleteVM.Get());
    }
    {
        CSQLAutoReset autoStatement(m_stmtDeleteAllModulesOfVM.Get());
        SQLBindInt64(m_stmtDeleteAllModulesOfVM.Get(), id, 1);
        SQLExecute(m_stmtDeleteAllModulesOfVM.Get());
    }    
}
static void InternalReadFields(CSQLStatement & statement, VmInfo * pInfo)
{
    pInfo->id = SQLReadInt64(statement.Get(), 0);
    pInfo->name = SQLReadWideString(statement.Get(), 1);
    pInfo->creationTime.InitFromSQL(SQLReadUtf8String(statement.Get(), 2));
    pInfo->lastWriteTime.InitFromSQL(SQLReadUtf8String(statement.Get(), 3));
}
void CVMDatabase::QueryVirtualMachines(IVmInfoListTarget * pVmListTarget)
{
    CSQLAutoReset autoStatement(m_stmtSelectVMs.Get());
    std::vector<VmInfo> result;
    result.push_back(VmInfo());
    VmInfo & info = result.front();
    for(;;)
    {
        if (!SQLStep(m_stmtSelectVMs.Get()))
        {
            break;
        }
        InternalReadFields(m_stmtSelectVMs, &info);
        pVmListTarget->OnData(result);
    }
}
bool CVMDatabase::QueryVMInfo(long long id, VmInfo * pVmInfo)
{
    CSQLAutoReset autoStatement(m_stmtSelectVMByID.Get());
    SQLBindInt64(m_stmtSelectVMByID.Get(), id, 1);
    if (!SQLStep(m_stmtSelectVMByID.Get()))
    {
        return false;
    }
    InternalReadFields(m_stmtSelectVMByID, pVmInfo);
    return true;
}

// modules
long long CVMDatabase::AddNewModule(long long vmId)
{
    CSQLTransaction transaction;
    transaction.Init(m_pDatabase->Get());
    long long newId = 1;
    {
        CSQLAutoReset autoStatement(m_stmtSelectMaxModulePosByVM.Get());
        SQLBindInt64(m_stmtSelectMaxModulePosByVM.Get(), vmId, 1);
        if (SQLStep(m_stmtSelectMaxModulePosByVM.Get()))
        {
            long long maxId = SQLReadInt64(m_stmtSelectMaxModulePosByVM.Get(), 0);
            if (maxId < 0 || maxId == MAXLONGLONG)
            {
                throw std::runtime_error("Invalid VM state");
            }
            newId = maxId + 1;
        }
    }

    {
        CSQLAutoReset autoStatement(m_stmtInsertModule.Get());
        SQLBindInt64(m_stmtInsertModule.Get(), vmId, 1);
        SQLBindInt64(m_stmtInsertModule.Get(), newId, 2);
        SQLExecute(m_stmtInsertModule.Get());
    }
    UpdateVMWriteTime(vmId);
    transaction.Commit();
    return newId;
}
void CVMDatabase::DelModule(long long vmId, 
                            long long moduleId)
{
    CSQLAutoReset autoStatement(m_stmtDeleteModule.Get());
    SQLBindInt64(m_stmtDeleteModule.Get(), vmId, 1);
    SQLBindInt64(m_stmtDeleteModule.Get(), moduleId, 2);
    SQLExecute(m_stmtDeleteModule.Get());
    UpdateVMWriteTime(vmId);
}
static void InternalReadFields(CSQLStatement & statement,
                               bool withData,
                               VmModuleInfo * pInfo)
{
    pInfo->id = SQLReadInt64(statement.Get(), 0);
    pInfo->descriptionXmlUtf8 = SQLReadUtf8String(statement.Get(), 1);
    pInfo->flags = SQLReadInt64(statement.Get(), 2);
    if (withData)
    {
        SQLReadBlob(statement.Get(), 3, &pInfo->data);
    }
}
void CVMDatabase::QueryModules(long long vmId,
                               bool queryData,
                               IVmModuleInfoListTarget * pVmListTarget)
{
    CSQLStatement & statement = queryData? m_stmtSelectModulesByVM :  m_stmtSelectModulesByVM_Nodata;

    CSQLAutoReset autoStatement(statement.Get());
    SQLBindInt64(statement.Get(), vmId, 1);
    std::vector<VmModuleInfo> result;
    result.push_back(VmModuleInfo());
    VmModuleInfo & info = result.front();
    for(;;)
    {
        if (!SQLStep(statement.Get()))
        {
            break;
        }
        InternalReadFields(statement, queryData, &info);
        pVmListTarget->OnData(result);
    }
}
bool CVMDatabase::QueryModuleInfo(long long vmId,
                                  long long moduleId,
                                  bool queryData,
                                  VmModuleInfo * pVmInfo)
{
    CSQLStatement & statement = queryData? m_stmtSelectModuleByID :  m_stmtSelectModuleByID_Nodata;

    CSQLAutoReset autoStatement(statement.Get());
    SQLBindInt64(statement.Get(), vmId, 1);
    SQLBindInt64(statement.Get(), moduleId, 2);
    if (!SQLStep(statement.Get()))
    {
        return false;
    }
    InternalReadFields(statement, queryData, pVmInfo);
    return true;
}
void CVMDatabase::UpdateModuleData(long long vmId,
                                   long long moduleId,
                                   const std::string & description,
                                   const std::vector<char> & data)
{
    CSQLAutoReset autoStatement(m_stmtUpdateModule.Get());
    SQLBindInt64(m_stmtUpdateModule.Get(), vmId, 1);
    SQLBindInt64(m_stmtUpdateModule.Get(), moduleId, 2);
    SQLBindUtf8String(m_stmtUpdateModule.Get(), description, 3);
    SQLBindBlob(m_stmtUpdateModule.Get(), data, 4);
    SQLExecute(m_stmtUpdateModule.Get());
    UpdateVMWriteTime(vmId);
}

void CVMDatabase::UpdateModuleXML(long long vmId,
                                  long long moduleId,
                                   const std::string & description)
{
    CSQLAutoReset autoStatement(m_stmtUpdateModuleXML.Get());
    SQLBindInt64(m_stmtUpdateModuleXML.Get(), vmId, 1);
    SQLBindInt64(m_stmtUpdateModuleXML.Get(), moduleId, 2);
    SQLBindUtf8String(m_stmtUpdateModuleXML.Get(), description, 3);
    SQLExecute(m_stmtUpdateModuleXML.Get());
    UpdateVMWriteTime(vmId);
}

void CVMDatabase::UpdateModuleFlags(long long vmId,
                                    long long moduleId,
                                    int flagsToSet,
                                    int flagsToRemove)
{
    unsigned long long argToAnd = ~(unsigned long long)flagsToRemove;
    argToAnd &= 0x7FFFFFFFFFFFFFFFULL;
    CSQLAutoReset autoStatement(m_stmtUpdateModuleFlags.Get());
    SQLBindInt64(m_stmtUpdateModuleFlags.Get(), vmId, 1);
    SQLBindInt64(m_stmtUpdateModuleFlags.Get(), moduleId, 2);
    SQLBindInt64(m_stmtUpdateModuleFlags.Get(), (long long)argToAnd, 3);
    SQLBindInt64(m_stmtUpdateModuleFlags.Get(), flagsToSet, 4);
    SQLExecute(m_stmtUpdateModuleFlags.Get());
    UpdateVMWriteTime(vmId);
}
void CVMDatabase::UpdateVMWriteTime(long long vmId)
{
    CSQLAutoReset autoStatement(m_stmtUpdateVMTimeByID.Get());
    SQLBindInt64(m_stmtUpdateVMTimeByID.Get(), vmId, 1);
    SQLExecute(m_stmtUpdateVMTimeByID.Get());
}

}