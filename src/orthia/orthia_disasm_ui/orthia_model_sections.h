#pragma once
#include "orthia_model_interfaces.h"

namespace orthia
{

void QuerySectionsImpl(IMemoryReader* reader, Address_type moduleBase, std::vector<SectionInfo>& sections_out);

}
