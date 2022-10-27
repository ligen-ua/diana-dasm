#ifndef ORTHIA_STACK_ANALYZER_H
#define ORTHIA_STACK_ANALYZER_H

#include "orthia_interfaces.h"

namespace orthia
{

void AnalyzeStack(orthia::IMemoryReader * pMemoryReader,
                  orthia::Address_type eipValue,
                  orthia::Address_type espValue,
                  orthia::Address_type levelsCount,
                  int mode);

}


#endif
