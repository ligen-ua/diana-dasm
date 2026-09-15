#include "cmd_common.h"


int wmain(int argc, wchar_t * argv[])
{
    try
    {
        Diana_Init();
        if (argc == 1)
        {
            orthia::PrintUsage();
            return 0;
        }
        if (wcscmp(argv[1], L"dump") == 0)
        {
            return orthia::ParseAndRunDump(argc, argv);
        }
        if (wcscmp(argv[1], L"disasm") == 0)
        {
            return orthia::ParseAndRunDisasm(argc, argv);
        }
        std::wcerr << L"Unknown command: "<<argv[1]<<"\n";
        return 1;
    }
    catch (std::exception& e)
    {
        std::wcerr << orthia::Utf8ToUtf16(e.what());
        return 1;
    }
    return 0;
}
