#pragma once

#include "dd_process_utils.h"

namespace dd
{

ULONGLONG HookProcess(const ProcessInfo & process, 
                 ULONGLONG patchAddress, 
                 int addressReg_number,
                 int sizeReg_number,
                 const std::string & outPath,
                 ULONG samplesCount);

void SelfTest();
}