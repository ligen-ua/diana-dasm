#ifndef ORTHIA_MODULE_MANAGER_H
#define ORTHIA_MODULE_MANAGER_H

#include "orthia_utils.h"
#include "orthia_interfaces.h"

namespace orthia
{

struct IMemoryReader;
ORTHIA_PREDECLARE(class CDatabaseManager);

class CModuleManager
{
    CModuleManager(const CModuleManager&);
    CModuleManager&operator = (const CModuleManager&);

    mutable CCriticalSection m_lock;
    mutable orthia::intrusive_ptr<CDatabaseManager> m_pDatabaseManager;
    std::wstring m_fullFileName;
public:
    CModuleManager();
    void Reinit(const std::wstring & fullFileName, bool bForce);
    std::wstring GetDatabaseName() const;
    orthia::intrusive_ptr<CDatabaseManager> QueryDatabaseManager(); 

    void UnloadModule(Address_type offset);
    void ReloadModule(Address_type offset,
                      IMemoryReader * pMemoryReader,
                      bool bForce,
                      const std::wstring & name,
                      int analyserFlags);
    void ReloadRange(Address_type offset,
                     Address_type size,
                     IMemoryReader * pMemoryReader,
                     int mode,
                     int analyserFlags);

    void QueryLoadedModules(std::vector<CommonModuleInfo> * pResult) const;
    // references
    void QueryReferencesFromInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pResult) const;
    void QueryReferencesToInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pResult) const;
    void QueryReferencesToInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult) const;
    void QueryReferencesFromInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult) const;
};

}
#endif