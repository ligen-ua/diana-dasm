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

typedef struct _DianaTextOutputContext
{
    DianaTextOutput_type pTextOutputFnc;
    DianaOpOutput_type pOpOutputFnc;
    DianaTextReset_type pReset;
    DianaParserResult * pResult;
    OPERAND_SIZE instructionRIP;
    int flags;
    int spacesCount;
}DianaTextOutputContext;

void DianaTextOutputContext_Init(DianaTextOutputContext * pContext,
                                DianaTextOutput_type pTextOut,
                                DianaOpOutput_type pOpOut,
                                int spacesCount);

typedef struct _DianaStringOutputContext
{
    DianaTextOutputContext parent;
    char * pBuffer;
    OPERAND_SIZE curSize;
    OPERAND_SIZE maxBufferSize;
}DianaStringOutputContext;

void DianaStringOutputContext_Init(DianaStringOutputContext * pContext,
                                   DianaTextOutput_type pTextOut,
                                   DianaOpOutput_type pOpOut,
                                   int spacesCount,
                                   char * pBuffer,
                                   OPERAND_SIZE maxBufferSize);

//  text out function
int DianaTextOutputContext_TextOut(DianaTextOutputContext * pContext,
                                   DianaParserResult * pResult, 
                                   OPERAND_SIZE instructionRIP);


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