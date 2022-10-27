#include "dd_out.h"
#include "orthia_utils.h"

namespace dd
{

struct OutParameters
{
    bool enabled;
    std::string prefix;
};

static OutParameters g_params[otCOUNT] = {
    {true, "ERROR: "},  // error
    {true, ""},        // info
    {false, "DEBUG: "} // debug
};

bool GetOutStatus(OutType_type type, std::string * pPrefix)
{
    OutParameters param = g_params[type];
    *pPrefix = param.prefix;
    return param.enabled;
}
void VerboseDebugOn()
{
    g_params[otDebug].enabled = true;
}

static orthia::CCriticalSection g_lock;

void AppendStream(std::ostream & ostream, std::string & data)
{
    orthia::CAutoCriticalSection guard(g_lock);
    ostream<<data;
}

base_out & base_out::operator << (const std::wstring & data)
{
    m_impl<<orthia::Utf16ToUtf8(data);
    return *this;
}

}