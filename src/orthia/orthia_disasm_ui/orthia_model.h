#pragma once

#include "orthia_utils.h"

namespace orthia
{
    class CProgramModel
    {
    public:
        void QueryWorkspace(std::vector<std::string>& allNames, std::string& active);
    };
}