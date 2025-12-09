#pragma once

#include <functional>
#include "oui_string.h"
namespace oui
{
    void SetupWin32DllLookupHandler(std::function<std::tuple<int, oui::String>(const oui::String& name)> dllLookupHandler);
}