#ifndef DIANA_OPERANDS_H
#define DIANA_OPERANDS_H

#include "diana_core.h"

DI_CHAR Diana_GetRm2(DianaContext * pContext, unsigned char postByte);

int Diana_LinkOperands(DianaContext * pContext, //IN
                       DianaParserResult * pResult, 
                       DianaReadStream * readStream);


#endif