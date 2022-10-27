#include "diana_text_output_masm.h"
#include "stdio.h"
#include "string.h"
#include "search.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#pragma warning(disable:4996)
#endif

static int GetRealSize(OPERAND_SIZE operand)
{
    if (operand <= 0xFFUL)
       return 1;
    if (operand <= 0xFFFFUL)
       return 2;
    if (operand <= 0xFFFFFFFFUL)
       return 4;
    return 8;
}
 
int DianaTextOutput_Console(struct _DianaTextOutputContext * pContext, 
                                    const char * pText)
{
    &pContext;
    printf("%s", pText);
    return DI_SUCCESS;
}
int DianaOpOutput_Console(struct _DianaTextOutputContext * pContext, 
                                  OPERAND_SIZE operand, 
                                  int size)
{
    &pContext;
    if (size == 0)
    {
        size = GetRealSize(operand);
    }
    switch(size)
    {
       case 1: if ((operand&0xF0ULL) >= 0xA0ULL && (operand&0xF0ULL) <= 0xF0ULL)
               {
                   printf("%s", "0");
               }
               printf("%0hhX", (unsigned char)operand);
           break;
       case 2: if ((operand&0xF000ULL) >= 0xA000ULL && (operand&0xF000ULL) <= 0xF000ULL)
               {
                   printf("%s", "0");
               }
               printf("%02hX", (unsigned short)operand);
           break;
       case 4: if ((operand&0xF0000000ULL) >= 0xA0000000ULL && (operand&0xF0000000ULL) <= 0xF0000000ULL)
               {
                   printf("%s", "0");
               }
               printf("%04X", (unsigned int)operand);
           break;
       case 8: if ((operand&0xF000000000000000ULL) >= 0xA000000000000000ULL && (operand&0xF000000000000000ULL) <= 0xF000000000000000ULL)
               {
                   printf("%s", "0");
               }
               printf("%08llX", operand);
           break;
       default:
           return DI_ERROR;
    }
    return DI_SUCCESS;
}

int DianaTextOutput_String(struct _DianaTextOutputContext * pContextIn, 
                           const char * pText)
{
    DianaStringOutputContext * pContext = (DianaStringOutputContext * )pContextIn;
    char * pBuffer = pContext->pBuffer + pContext->curSize;
    OPERAND_SIZE tail = 0;
    int written = 0;
    
    if (pContext->curSize >= pContext->maxBufferSize)
        return DI_ERROR;
    
    tail = pContext->maxBufferSize - pContext->curSize;
    written = snprintf(pBuffer, (size_t)tail, "%s", pText);
    pContext->curSize += written;
    return DI_SUCCESS;
}
static int IsHexAlpha(char ch)
{
    if (ch >= 'A' && ch <='F')
        return 1;
    if (ch >= 'a' && ch <='f')
        return 1;
    return 0;
}
int DianaOpOutput_String(struct _DianaTextOutputContext * pContextIn, 
                                  OPERAND_SIZE operand, 
                                  int size)
{
    DianaStringOutputContext * pContext = (DianaStringOutputContext * )pContextIn;
    char * pBuffer = pContext->pBuffer + pContext->curSize;
    OPERAND_SIZE tail = 0;
    int written = 0;
    
    if (pContext->curSize >= pContext->maxBufferSize)
        return DI_ERROR;
        
    tail = pContext->maxBufferSize - pContext->curSize;
    if (size == 0)
    {
        size = GetRealSize(operand);
    }
    switch(size)
    {
       case 1: written = snprintf(pBuffer, (size_t)tail, "%0hhX", (unsigned char)operand);
           break;
       case 2: written = snprintf(pBuffer, (size_t)tail, "%02hX", (unsigned short)operand);
           break;
       case 4: written = snprintf(pBuffer, (size_t)tail, "%04X", (unsigned int)operand);
           break;
       case 8: written = snprintf(pBuffer, (size_t)tail, "%08llX", operand);
           break;
       default:
           return DI_ERROR;
    }
    if (IsHexAlpha(pBuffer[0]))
    {
       if (written + 2>= tail)
       {
          return DI_ERROR;
       }
       ++written;
       memmove(pBuffer+1, pBuffer, written);
       pBuffer[0] = '0';
    }
    pContext->curSize += written;
    return DI_SUCCESS;
}