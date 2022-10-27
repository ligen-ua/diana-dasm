#ifndef DIANA_ANALYZE_H
#define DIANA_ANALYZE_H

#include "diana_stack.h"


#define DI_INSTRUCTION_CAN_CHANGE_CSIP    0x1
#define DI_INSTRUCTION_IS_RET             0x2
#define DI_INSTRUCTION_IS_LOADING         0x4
#define DI_INSTRUCTION_USES_RIP           0x8
#define DI_INSTRUCTION_USES_UNKNOWN_RIP   0x10
#define DI_INSTRUCTION_INVALID            0x20
#define DI_INSTRUCTION_ROOT               0x40
#define DI_INSTRUCTION_EXTERNAL           0x80
#define DI_INSTRUCTION_DATA               0x100

#define DI_ROUTE_QUESTIONABLE             0x1

#define DI_XREF_INVALID                   0x1
#define DI_XREF_EXTERNAL                  0x2

typedef struct _Diana_Instruction
{
    // used only in contructror time
    Diana_FList_ListNode m_routeEntry;
    Diana_FList_List m_referencesFromThisInstruction;
    Diana_FList_List m_referencesToThisInstruction;
    OPERAND_SIZE m_offset;
    int m_flags;
}Diana_Instruction;

typedef struct _Diana_SubXRef
{
    Diana_FList_ListNode m_instructionEntry; // must be first
    Diana_Instruction * m_pInstruction;
}Diana_SubXRef;


typedef struct _Diana_XRef
{
    Diana_SubXRef m_subrefs[2];  // from -> to
    int m_flags;
}Diana_XRef;

typedef int (* DianaAnalyzeMoveTo_fnc)(void * pThis, OPERAND_SIZE offset);
// possible in flags
#define DIANA_ANALYZE_ABSOLUTE_ADDRESS  1

// possuble result:
typedef enum {diaJumpInvalid, diaJumpNormal, diaJumpExternal} DianaAnalyzeAddressResult_type;
typedef int (* DianaAnalyzeAddress_type)(void * pThis, 
                                   OPERAND_SIZE address,
                                   int flags,
                                   OPERAND_SIZE * pRelativeOffset,
                                   DianaAnalyzeAddressResult_type * pResult);

typedef struct _dianaAnalyzeObserver
{
    DianaMovableReadStream * m_pStream;
    DianaAnalyzeAddress_type m_pAnalyzeAddress;
}DianaAnalyzeObserver;

void DianaAnalyzeObserver_Init(DianaAnalyzeObserver * pThis,
                               DianaMovableReadStream * pStream,
                               DianaAnalyzeAddress_type pAnalyzeJumpAddress);

void Diana_Instruction_Init(Diana_Instruction * pInstruction,
                            OPERAND_SIZE m_offset,
                            int flags);

typedef struct _Diana_InstructionsOwnerBase
{
    int c;
}Diana_InstructionsOwnerBase;


typedef struct _Diana_InstructionsOwner
{
    Diana_InstructionsOwnerBase m_base;
    Diana_Stack m_xrefs;
    Diana_Instruction ** m_ppPresenceVec;
    Diana_Instruction * m_pInstructionsVec;
    OPERAND_SIZE m_maxSize;
    OPERAND_SIZE m_usedSize;
    OPERAND_SIZE m_actualSize;
    int m_stackInited;
    Diana_FList_List m_externalInstructionsList;
    OPERAND_SIZE m_minimalExternalAddress;
}Diana_InstructionsOwner;

typedef struct _diana_RouteInfo
{
    Diana_FList_List instructions;
    OPERAND_SIZE  startOffset;
    long flags;
}Diana_RouteInfo;

int Diana_InstructionsOwner_Init(Diana_InstructionsOwner * pOwner,
                                 OPERAND_SIZE maxOffsetSize,
                                 OPERAND_SIZE minimalExternalAddress);
void Diana_InstructionsOwner_Free(Diana_InstructionsOwner * pOwner);
Diana_Instruction * Diana_InstructionsOwner_AllocateInstruction(Diana_InstructionsOwner * pOwner);

int Diana_AnalyzeCode(Diana_InstructionsOwner * pOwner,
                      DianaAnalyzeObserver * pObserver,
                      int mode,
                      OPERAND_SIZE initialOffset,
                      OPERAND_SIZE maxOffset);

Diana_XRef * Diana_CastXREF(Diana_FList_ListNode * pNode,
                            int index);

#define DI_ANALYSE_BREAK_ON_INVALID_CMD   1
#define DI_ANALYSE_PE_FILE_SCAN_THROUGH   2

int Diana_AnalyzeCodeEx(Diana_InstructionsOwner * pOwner,
                        DianaAnalyzeObserver * pObserver,
                        int mode,
                        OPERAND_SIZE initialOffset,
                        OPERAND_SIZE maxOffset,
                        Diana_Stack * pStack,
                        int flags);
#endif