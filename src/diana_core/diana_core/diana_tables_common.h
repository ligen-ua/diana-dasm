#ifndef DIANA_TABLES_COMMON_H
#define DIANA_TABLES_COMMON_H

#include "diana_core.h"

int Diana_DispatchMod3(DianaLinkedOperand * pInfo,
                       int rm,
                       DI_CHAR old_true_rm,
                       int iRegSizeInBytes,
                       DianaUnifiedRegister *pOut,
                       int isRexPrefix);

#endif