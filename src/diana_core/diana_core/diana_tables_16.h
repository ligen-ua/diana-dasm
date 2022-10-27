#ifndef DIANA_TABLES_16_H
#define DIANA_TABLES_16_H

#include "diana_core.h"

int Diana_ReadIndexStructure16(DianaContext * pContext,
                               DianaLinkedOperand * pInfo,
                               DI_CHAR iOpSize,
                               unsigned char postbyte,
                               DianaReadStream * pStream, 
                               DianaOperandValue * pValue,
                               DianaValueType * pType);

#endif