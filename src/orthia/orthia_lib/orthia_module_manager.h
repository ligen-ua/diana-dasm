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

    orthia::intrusive_ptr<CDatabaseManager> m_pDatabaseManager;
    std::wstring m_fullFileName;
public:
    CModuleManager();
    void Reinit(const std::wstring & fullFileName, bool bForce);
    std::wstring GetDatabaseName();
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

    void QueryLoadedModules(std::vector<CommonModuleInfo> * pResult);
    // references
    void QueryReferencesFromInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pResult);
    void QueryReferencesToInstruction(Address_type offset, std::vector<CommonReferenceInfo> * pResult);
    void QueryReferencesToInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult);
    void QueryReferencesFromInstructionsRange(Address_type address1, Address_type address2, std::vector<CommonRangeInfo> * pResult);
};

}
#endif