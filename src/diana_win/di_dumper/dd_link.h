#pragma once

namespace dd
{
class ProcessInfo;
struct RegionInfo;

void LoadRemoteFunctions(const ProcessInfo & process, 
                         RegionInfo & region);

}