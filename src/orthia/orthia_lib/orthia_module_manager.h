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

    mutable CCriticalSection m_dbLock;
    mutable CCriticalSection m_writeLock;
    mutable orthia::intrusive_ptr<CDatabaseManager> m_pDatabaseManager;
    orthia::PlatformString_type m_fullFileName;
public:
    CModuleManager();
    void Reinit(const orthia::PlatformString_type & fullFileName, bool bForce);
    orthia::PlatformString_type GetDatabaseName() const;
    orthia::intrusive_ptr<CDatabaseManager> QueryDatabaseManager() const; 

    void UnloadModule(Address_type offset);
    void ReloadModule(Address_type offset,
                      IMemoryReader * pMemoryReader,
                      bool bForce,
                      const orthia::PlatformString_type & name,
                      int analyserFlags);
    void ReloadModuleWithHints(Address_type offset,
                               IMemoryReader * pMemoryReader,
                               const orthia::PlatformString_type & name,
                               int analyserFlags,
                               const std::vector<Address_type> & hints);
    void ReloadRange(Address_type offset,
                     Address_type size,
                     IMemoryReader * pMemoryReader,
                     int mode,
                     int analyserFlags);

    void QueryLoadedModules(std::vector<CommonModuleInfo> * pResult) const;
    // references
    Address_type QueryRouteStart(Address_type offset);
    void QueryReferencesFromInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pResult) const;
    void QueryReferencesToInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pResult) const;
    void QueryReferencesToInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult) const;
    void QueryReferencesFromInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult) const;
};

}
#endif