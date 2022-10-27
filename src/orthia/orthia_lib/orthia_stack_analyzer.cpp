#include "orthia_stack_analyzer.h"
#include "orthia_module_manager.h"
#include "orthia_memory_cache.h"
#include "diana_core_cpp.h"
#include "orthia_streams.h"
#include "orthia_memory_cache.h"

extern "C"
{
#include "diana_analyze2.h"
}


namespace orthia
{


void AnalyzeStack(orthia::IMemoryReader * pMemoryReader,
                  orthia::Address_type eipValue,
                  orthia::Address_type espValue,
                  orthia::Address_type levelsCount,
                  int mode)
{
}


}