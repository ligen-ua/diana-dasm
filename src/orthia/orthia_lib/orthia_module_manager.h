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
    // registers the module row without code analysis (quick open)
    void RegisterModule(Address_type offset,
                        IMemoryReader * pMemoryReader,
                        const orthia::PlatformString_type & name);
    // analyzes a module registered earlier and adds its references, keeping its metainfo
    void AnalyzeRegisteredModule(Address_type offset,
                                 IMemoryReader * pMemoryReader,
                                 const orthia::PlatformString_type & name,
                                 int analyserFlags);
    void ReloadModuleWithHints(Address_type offset,
                               IMemoryReader * pMemoryReader,
                               const orthia::PlatformString_type & name,
                               int analyserFlags,
                               const std::vector<Address_type> & hints);
    // the full analysis of a module, registered or not: from its own entry points and from the
    // calls and jumps other modules saved into it. Returns how many of those it started from
    size_t AnalyzeModuleWithIncoming(Address_type offset,
                                     IMemoryReader * pMemoryReader,
                                     const orthia::PlatformString_type & name,
                                     int analyserFlags);
    // code without headers: mode is the pointer size, arch DIANA_ARCH_*,
    // analyserFlags DI_ANALYSE_PE_FILE_SCAN_THROUGH to look at every place nothing refers to
    void ReloadRange(Address_type offset,
                     Address_type size,
                     IMemoryReader * pMemoryReader,
                     int mode,
                     int arch,
                     int analyserFlags);

    // a range without headers registered without analysis (a floating module); nothing when it exists
    void RegisterRange(Address_type offset,
                       Address_type size,
                       const orthia::PlatformString_type & name);
    // analyzes a registered range from its entry points (none inside it: from its start) and adds
    // the references it finds, keeping the module row and its metainfo. Returns the lowest and the
    // highest instruction start found, both 0 when none
    std::pair<Address_type, Address_type> AnalyzeRange(Address_type offset,
                      Address_type size,
                      IMemoryReader * pMemoryReader,
                      int mode,
                      int arch,
                      int analyserFlags,
                      const std::vector<Address_type> & entryPoints,
                      const orthia::PlatformString_type & name);
    // the references made from inside the module, for a full analysis again; the module row and its
    // metainfo (exports, symbols, flags) stay
    void ClearModuleReferences(Address_type offset);

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