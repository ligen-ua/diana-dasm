#include "orthia_utils.h"
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "orthia_files.h"

namespace orthia
{

int GetAppDataFolderWithSlash_Silent(PlatformString_type& result)
{
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg && xdg[0] != '\0')
    {
        result = xdg;
    }
    else
    {
        const char* home = getenv("HOME");
        if (!home || home[0] == '\0')
        {
            return ENOENT;
        }
        result = home;
        result += "/.local/share";
    }
    EnsureLastSlash(result);
    return 0;
}

void CreateAllDirectoriesForFile(const PlatformString_type& fullFileName)
{
    for (size_t i = 1; i < fullFileName.size(); ++i)
    {
        if (fullFileName[i] == '/')
        {
            PlatformString_type path = fullFileName.substr(0, i + 1);
            mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
        }
    }
}

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

void* GetCurrentProcessModule(void)
{
    Dl_info info;
    /* Pass the address of this function itself as a reference point */
    if (dladdr((void*)&GetCurrentProcessModule, &info) == 0)
        return NULL;

    /* info.dli_fbase is the load address (base) of the ELF mapping.
       On Linux this IS the ELF header: it starts with \x7fELF */
    return info.dli_fbase;
}

}