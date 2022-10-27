#pragma once

#include "orthia_memory_cache.h"
#include "diana_core_cpp.h"
extern "C"
{
#include "diana_processor/diana_processor_context.h"
#include "diana_processor/diana_processor_core.h"
}
#include "orthia_plugin_interfaces.h"
#include "orthia_commands.h"
#include "orthia_common_print.h"
namespace orthia
{

std::string QueryRegName(const std::string & base);
orthia::Address_type QueryRegValue(const std::string & base);

void PrintResult(_Diana_Processor_Registers_Context * pContext, 
                 int emulationResult,
                 int dianaMode);
void PrintPage(orthia::Address_type addressOfPage,
               const std::vector<char> & data,
               int dianaMode);
void PrintData(orthia::CMemoryStorageOfModifiedData & allWrites,
               int dianaMode);


std::wstring UnescapeArg(const std::wstring & value);

const char * ReadExpressitonValue(const char * args, 
                                  long long & value, 
                                  bool exactMatch = true);
const char * ReadExpressitonValue(const char * args, 
                                  orthia::Address_type & value, 
                                  bool exactMatch = true);
const char * ReadExpressitonValue(const char * args, 
                                  std::string & value, 
                                  bool exactMatch = true);


class CPrintfWriter:public orthia::ITextPrinter
{
public:
    virtual void PrintLine(const std::wstring & line);
};

const char * ReadWindbgSize(const char * pTail, 
                            orthia::Address_type * pSize,
                            bool exactMatch = true,
                            bool checkPrefixAndSilentlyFail = false);

}