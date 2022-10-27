#ifndef ORTHIA_DATABASE_MODULE_H
#define ORTHIA_DATABASE_MODULE_H

#include "orthia_utils.h"
#include "orthia_interfaces.h"
#include "orthia_sqlite_utils.h"
#include "orthia_database_impl_classic.h"
#include "orthia_database_impl_vm.h"


namespace orthia
{

struct CommonReferenceInfo;

class CDatabase;
class CDBVersion:public CBaseOperation
{
    orthia::CSQLStatement m_getDBVersion;
    orthia::CSQLStatement m_addDBVersion;
public:
    CDBVersion();
    void Init(sqlite3 * database);
    int GetDBVersion();
    void AddDBVersion(int newVersion);
};

class CDatabase:public orthia::RefCountedBase
{
    CDatabase(const CDatabase &);
    CDatabase & operator =(const CDatabase &);

    orthia::intrusive_ptr<CSQLDatabase2> m_pDatabase;
    CDBVersion  m_dbVersion;
    orthia::intrusive_ptr<CClassicDatabase> m_pClassicDatabase;
    orthia::intrusive_ptr<CVMDatabase> m_pVMDatabase;

    void Init();
    void DoVersionScripts();
    void DoUpdate_0_1();
public:
    CDatabase();
    ~CDatabase();
    // file api
    void CreateNew(const std::wstring & fullFileName);
    void OpenExisting(const std::wstring & fullFileName);

    orthia::intrusive_ptr<CClassicDatabase> GetClassicDatabase();
    orthia::intrusive_ptr<CVMDatabase> GetVMDatabase();
};

class CDatabaseManager:public orthia::RefCountedBase
{
    CDatabaseManager(const CDatabaseManager &);
    CDatabaseManager & operator =(const CDatabaseManager &);

    orthia::intrusive_ptr<CDatabase> m_database;
public:
    CDatabaseManager();
    ~CDatabaseManager();
    // file api
    void CreateNew(const std::wstring & fullFileName);
    void OpenExisting(const std::wstring & fullFileName);

    orthia::intrusive_ptr<CDatabase> GetDatabase();
    orthia::intrusive_ptr<CClassicDatabase> GetClassicDatabase();
    orthia::intrusive_ptr<CVMDatabase> GetVMDatabase();
};

ORTHIA_DECLARE(CDatabase);
ORTHIA_DECLARE(CDatabaseManager);

}
#endif
