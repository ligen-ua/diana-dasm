#ifndef DIANA_TEXT_OUTPUT_MASM_H
#define DIANA_TEXT_OUTPUT_MASM_H

#include "diana_core.h"
struct _DianaTextOutputContext;
typedef int (*DianaTextOutput_type)(struct _DianaTextOutputContext * pContext, 
                                    const char * pText);
typedef int (*DianaOpOutput_type)(struct _DianaTextOutputContext * pContext, 
                                  OPERAND_SIZE operand, 
                                  int size);
typedef int (*DianaTextReset_type)(struct _DianaTextOutputContext * pContext);

// DianaTextOutputContext::flags, set by the printer for the current instruction
// numbers passed to pOpOutputFnc should be printed as 0x1234 instead of 1234h (ARM)
#define DIANA_TEXT_OUTPUT_FLAG_C_HEX            0x100

// DianaTextOutputContext::options, set by the user
// ARM: print raw instructions, no aliases (orr instead of mov, ubfm instead of lsl...)
#define DIANA_TEXT_OUTPUT_OPTION_ARM_NO_ALIASES 0x1

typedef struct _DianaTextOutputContext
{
    DianaTextOutput_type pTextOutputFnc;
    DianaOpOutput_type pOpOutputFnc;
    DianaTextReset_type pReset;
    DianaParserResult * pResult;
    OPERAND_SIZE instructionRIP;
    int flags;
    int spacesCount;
    int options;
}DianaTextOutputContext;

void DianaTextOutputContext_Init(DianaTextOutputContext * pContext,
                                DianaTextOutput_type pTextOut,
                                DianaOpOutput_type pOpOut,
                                int spacesCount);

struct _DianaStringOutputContext;
typedef void (*DianaStringOutputOnOpCallback_type)(struct _DianaStringOutputContext* pContext,
    void * pUserContext, 
    const char* pText, int size,
    OPERAND_SIZE operand, int opSize);

typedef struct _DianaStringOutputContext
{
    DianaTextOutputContext parent;
    char * pBuffer;
    OPERAND_SIZE curSize;
    OPERAND_SIZE maxBufferSize;
    DianaStringOutputOnOpCallback_type pOnOpCallback;
    void* onOpCallbackContext;

}DianaStringOutputContext;

void DianaStringOutputContext_Init(DianaStringOutputContext * pContext,
                                   DianaTextOutput_type pTextOut,
                                   DianaOpOutput_type pOpOut,
                                   int spacesCount,
                                   char * pBuffer,
                                   OPERAND_SIZE maxBufferSize);

//  text out function, x86 and ARM
int DianaTextOutputContext_TextOut(DianaTextOutputContext * pContext,
                                   DianaParserResult * pResult,
                                   OPERAND_SIZE instructionRIP);

// ARM part, called by DianaTextOutputContext_TextOut, see diana_text_output_arm.c
int DianaA64_TextOut(DianaTextOutputContext * pContext);
int DianaA32_TextOut(DianaTextOutputContext * pContext);


// c-runtime
int DianaTextOutput_Console(struct _DianaTextOutputContext * pContext,
                                    const char * pText);
int DianaOpOutput_Console(struct _DianaTextOutputContext * pContext,
                                  OPERAND_SIZE operand,
                                  int size);
int DianaTextOutput_String(struct _DianaTextOutputContext * pContext,
                                    const char * pText);
int DianaOpOutput_String(struct _DianaTextOutputContext * pContext,
                                  OPERAND_SIZE operand,
                                  int size);

#endif