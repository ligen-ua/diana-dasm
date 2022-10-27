#ifndef ORTHIA_DATABASE_IMPL_VM_H
#define ORTHIA_DATABASE_IMPL_VM_H

#include "orthia_utils.h"
#include "orthia_interfaces.h"
#include "orthia_sqlite_utils.h"
#include "orthia_interfaces_vm.h"

namespace orthia
{

class CVMDatabase:public orthia::RefCountedBase
{
    intrusive_ptr<CSQLDatabase2> m_pDatabase;
    CVMDatabase(const CVMDatabase &);
    CVMDatabase & operator =(const CVMDatabase &);

    CSQLStatement m_stmtSelectRowId;
    // vms
    CSQLStatement m_stmtInsertVM, m_stmtInsertVM_withID;
    CSQLStatement m_stmtDeleteVM;
    CSQLStatement m_stmtSelectVMs;
    CSQLStatement m_stmtSelectVMByID;
    CSQLStatement m_stmtSelectVMByName;
    CSQLStatement m_stmtUpdateVMTimeByID;

    // modules
    CSQLStatement m_stmtSelectMaxModulePosByVM;
    CSQLStatement m_stmtInsertModule;
    CSQLStatement m_stmtDeleteModule;
    CSQLStatement m_stmtDeleteAllModulesOfVM;
    CSQLStatement m_stmtSelectModulesByVM;
    CSQLStatement m_stmtSelectModuleByID;
    CSQLStatement m_stmtUpdateModule;
    CSQLStatement m_stmtUpdateModuleXML;
    CSQLStatement m_stmtUpdateModuleFlags;

    CSQLStatement m_stmtSelectModulesByVM_Nodata;
    CSQLStatement m_stmtSelectModuleByID_Nodata;

    void Init();
    long long QueryLastId();

    void UpdateVMWriteTime(long long vmId);
public:
    CVMDatabase(intrusive_ptr<CSQLDatabase2> pDatabase);
    ~CVMDatabase();

    long long AddNewVM(const std::wstring & name, const long long * pID = 0);
    void DelVM(long long id);
    void QueryVirtualMachines(IVmInfoListTarget * pVmListTarget);
    bool QueryVMInfo(long long id, VmInfo * pVmInfo);

    // modules
    long long AddNewModule(long long vmId);
    void DelModule(long long vmId, long long moduleId);
    void QueryModules(long long vmId,  
                      bool queryData,
                      IVmModuleInfoListTarget * pVmListTarget);
    bool QueryModuleInfo(long long vmId,
                         long long moduleId,
                         bool queryData,
                         VmModuleInfo * pVmInfo);
    void UpdateModuleData(long long vmId,
                          long long moduleId,
                          const std::string & description,
                          const std::vector<char> & data);
    void UpdateModuleXML(long long vmId,
                         long long moduleId,
                         const std::string & description);
    void UpdateModuleFlags(long long vmId,
                           long long moduleId,
                           int flagsToSet,
                           int flagsToRemove);
    // --
    CSQLDatabase * GetDatabase() { return m_pDatabase.get(); }

};


ORTHIA_DECLARE(CVMDatabase);

}
#endif
