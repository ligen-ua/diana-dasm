#ifndef ORTHIA_VMLIB_H
#define ORTHIA_VMLIB_H

#include "orthia_interfaces.h"
#include "orthia_database_module.h"


struct _Diana_Processor_Registers_Context;

namespace orthia
{

long long DoCommand_vm_new(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           const std::wstring & name,
                           const long long * pID);
void DoCommand_vm_del(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                      long long id);
void DoCommand_vm_list(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                       IVmInfoListTarget * pVmListTarget);
bool DoCommand_vm_info(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                       long long id, 
                       VmInfo * pVmInfo);
void DoCommand_vm_query_mem(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                            long long vmId,
                            bool onlyEnabled,
                            unsigned long long startAddress,
                            unsigned long long size,
                            IMemoryReader * pReader,
                            IVmMemoryRangesTarget * pTarget);

long long DoCommand_vm_mod_new(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, long long vmId);
void DoCommand_vm_mod_del(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager,
                          long long vmId,
                          long long moduleId);

void DoCommand_vm_mod_list(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                          long long vmId,
                          IGUIVmModuleInfoListTarget * pVmModuleListTarget);
bool DoCommand_vm_mod_info(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           long long vmId,
                           long long moduleId,
                           GUIVmModuleInfo * pVmModuleInfo);

void DoCommand_vm_mod_write(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           long long vmId,
                           long long moduleId,
                           unsigned long long startAddress,
                           const std::vector<char> & buffer);

void DoCommand_vm_mod_rebuild_metadata(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                                       long long vmId,
                                       long long moduleId);

void DoCommand_vm_mod_enable(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                             long long vmId,
                             long long moduleId,
                             bool bEnable);

class CMemoryStorageOfModifiedData;
int DoCommand_vm_vm_call(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                         long long vmId,
                         unsigned long long startAddress,
                         long long commandsCount,
                         IMemoryReader * pReader,
                         int dianaMode,
                         _Diana_Processor_Registers_Context * pContext,
                         IDebugger * pDebugger,
                         orthia::CMemoryStorageOfModifiedData & allWrites,
                         long long * pCommandsCount,
                         std::vector<OPERAND_SIZE> * pCallStack,
                         IAPIHandlerDebugInterface * pAPIHandlerDebugInterface,
                         IAddressSpace * pAddressSpace);

int DoCommand_vm_mod_load(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           long long vmId,
                           long long moduleId,
                           unsigned long long moduleAddress,
                           const std::vector<char> & buffer,

                           IMemoryReader * pReader,
                           int dianaMode,
                           IDebugger * pDebugger,
                           diana::CBasePeLinkImportsObserver * pLinkObserver,
                           _Diana_Processor_Registers_Context * pContext,
                           std::vector<OPERAND_SIZE> * pCallStack,
                           IAddressSpace * pAddressSpace,
                           const std::wstring & fileNameHint,
                           IAPIHandlerDebugInterface * pAPIHandlerDebugInterface,
                           long long * pCommandsCount);

typedef std::set<std::wstring> AttributesToDeleteSet_type;
typedef std::map<std::wstring, std::wstring> AttributesToAddMap_type;
void DoCommand_vm_mod_manage_custom_attributes(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                                               long long vmId,
                                               long long moduleId,
                                               AttributesToDeleteSet_type & attributesToDelete,
                                               AttributesToAddMap_type & attributesToAdd);

int DoCommand_vm_mod_shcall(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           long long vmId,
                           long long moduleId,
                           const std::string & function,
                           IMemoryReader * pReader,
                           int dianaMode,
                           IDebugger * pDebugger,
                           orthia::CMemoryStorageOfModifiedData & allWrites,
                           diana::CBasePeLinkImportsObserver * pLinkObserver,
                           _Diana_Processor_Registers_Context * pContext,
                           long long * pCommandsCount,
                           std::vector<OPERAND_SIZE> * pCallStack,
                           IAddressSpace * pAddressSpace,
                           const std::vector<orthia::Address_type> & arguments,
                           IAPIHandlerDebugInterface * pAPIHandlerDebugInterface);

}


#endif
