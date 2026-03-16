#include "orthia_utils.h"
#include <sys/stat.h>
#include "orthia_files.h"

namespace orthia
{

PlatformString_type GetCurrentProcessDir()
{
    PlatformString_type res;
    res.resize(PATH_MAX);
    auto size = readlink("/proc/self/exe", res.data(), PATH_MAX-1);
    if (size == -1) 
    {
         ORTHIA_THROW_ERRNO("GetCurrentProcessDir failed");
    }
    res.resize(size);
    res = GetPathNameOfFullPathName2(res);
    EnsureLastSlash(res);
    return res;
}


}