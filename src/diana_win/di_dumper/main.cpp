#include <iostream>
#include <string>
#include "dd_process_utils.h"
#include "dd_hook.h"
#include "orthia_utils.h"
#include "dd_out.h"

static void PrintUsage()
{
    std::cout<<"Usage: 1) status <pid> [control_block_address]\n";
    std::cout<<"       2) inject <pid> <patch_address> --buffer <reg1-buffer> <reg2-size> --dir <out_directory> <number_of_samples> --debug \n";
}

static std::map<std::wstring, int> g_RegsMapping;
static std::map<int, std::string> g_ReverseRegsMapping;

static
int CaptureRegister(const std::wstring & regString)
{
    std::map<std::wstring, int>::const_iterator it = g_RegsMapping.find(regString);
    if (it == g_RegsMapping.end())
    {
        throw std::runtime_error("Unknown register: " + orthia::Utf16ToAcp(regString));
    }
    return it->second;
}

static
std::string GetRegistryById(int id)
{
    std::map<int, std::string>::const_iterator it = g_ReverseRegsMapping.find(id);
    if (it == g_ReverseRegsMapping.end())
    {
        return "Unknown (" + orthia::ObjectToString_Ansi(id) + ")";
    }
    return it->second;
}

static
ULONG CapturePid(const std::wstring & pidString)
{
    if (pidString.empty())
    {
        throw std::runtime_error("Invalid argument");
    }
    ULONG pid = 0;
    if (pidString.size() > 2 && pidString[0] == '0' && pidString[0] == 'x')
    {
        orthia::HexStringToObject(std::wstring(pidString.begin() + 2, pidString.end()), &pid);
        return pid;
    }
    orthia::StringToObject(pidString, &pid);
    return pid;
}
static
ULONGLONG CaptureAddress(const std::wstring & pidString)
{
    if (pidString.empty())
    {
        throw std::runtime_error("Invalid argument");
    }
    ULONGLONG pid = 0;
    if (pidString.size() > 2 && pidString[0] == '0' && pidString[0] == 'x')
    {
        orthia::HexStringToObject(std::wstring(pidString.begin() + 2, pidString.end()), &pid);
        return pid;
    }
    orthia::StringToObject(pidString, &pid);
    return pid;
}

static void PrintRegionInfo(const dd::RegionInfo & info)
{
    dd::info_out()<<"["<<std::hex<<info.baseAddress<<"]";
    dd::info_out()<<"    Hook: "<<std::hex<<info.parts.patchedAddress;
    dd::info_out()<<"    Live counter: "<<std::hex<<info.parts.liveCounter;
    dd::info_out()<<"    Samples count: "<<std::hex<<info.parts.samplesCount;
    dd::info_out()<<"    Error code: "<<std::hex<<info.parts.errorCode;
    dd::info_out()<<"    Directory: "<<info.GetDir();
    dd::info_out()<<"    Buffer: "<<GetRegistryById(info.parts.addressReg_number);
    dd::info_out()<<"    Directory: "<<GetRegistryById(info.parts.sizeReg_number);

    dd::info_out()<<"    Samples to go: "<<std::hex<<info.parts.samplesToProceed;
    dd::info_out()<<"    Samples saved: "<<std::hex<<info.parts.samplesReported;

    std::string status;
    if (info.parts.exitCommand)
    {
        status = "Exiting"; 
    }
    else if (info.parts.saverIsReady)
    {
        status = "Ready";
    }
    else
    {
        status = "Unknown";
    }
    dd::info_out()<<"    Status: "<<status;
}

static 
int StatusCommand(int argc, wchar_t * argv[])
{
    if (argc != 3 && argc != 4)
    {
        throw std::runtime_error("Usage: pid [address]");
    }
    ULONG pid = CapturePid(argv[2]);
    
    if (argc == 3)
    {
        dd::ProcessInfo processInfo;
        dd::OpenProcess(pid, false, &processInfo);

        std::vector<dd::RegionInfo> regions;
        ScanRegions(processInfo, &regions);

        for(std::vector<dd::RegionInfo>::const_iterator it = regions.begin(), it_end = regions.end();
            it != it_end;
            ++it)
        {
            PrintRegionInfo(*it);
        }
        return 0;
    }
    
    
    ULONGLONG address = CaptureAddress(argv[3]);

    dd::ProcessInfo processInfo;
    dd::OpenProcess(pid, false, &processInfo);

    dd::RegionInfo region;
    region.baseAddress = address;

    dd::LoadPageFromProcess(processInfo, region);
    PrintRegionInfo(region);
    return 0;
}

static 
int InjectCommand(int argc, wchar_t * argv[])
{
    if (argc < 4)
    {
        throw std::runtime_error("Illegal number of arguments, see usage");
    }

    int addressReg_number = 0;
    int sizeReg_number = 0;
    ULONG pid = CapturePid(argv[2]);
    ULONGLONG address = CaptureAddress(argv[3]);
    std::wstring dir;
    int numberOfSamples = 0;
    for(int i = 4; i < argc; ++i)
    {
        if (wcscmp(argv[i], L"--buffer")==0)
        {
            if (i + 2 >= argc)
            {
                throw std::runtime_error("Expected: register1 register2");
            }
            addressReg_number = CaptureRegister(argv[i+1]);
            sizeReg_number = CaptureRegister(argv[i+2]);
            i += 2;
            continue;
        }
        if (wcscmp(argv[i], L"--dir")==0)
        {
            if (i + 2 >= argc)
            {
                throw std::runtime_error("Expected: directory number_of_samples");
            }
            dir = argv[i+1];
            numberOfSamples = CapturePid(argv[i+2]);
            i += 2;
            continue;
        }
        if (wcscmp(argv[i], L"--debug")==0)
        {
            dd::VerboseDebugOn();
            continue;
        }
    }

    if (numberOfSamples || !dir.empty())
    {
        if (addressReg_number == 0 || sizeReg_number == 0)
        {
            throw std::runtime_error("Please specify the registers to capture");
        }
    }

    std::vector<wchar_t> fullBuf;
    orthia::GetFullPathNameX(dir, fullBuf, 0);
    if (!fullBuf.empty() && fullBuf[0])
    {
        dir = &fullBuf[0];
    }
    dd::debug_out()<<"Process: "<<std::hex<<pid;
    dd::debug_out()<<"Directory: "<<dir;
    dd::debug_out()<<"Number of samples: "<<numberOfSamples;
    dd::debug_out()<<"Buffer: "<<addressReg_number;
    dd::debug_out()<<"Size: "<<sizeReg_number;

    dd::ProcessInfo processInfo;
    dd::OpenProcess(pid, true, &processInfo);
    
    ULONGLONG res = dd::HookProcess(processInfo, 
                                        address, 
                                        addressReg_number,
                                        sizeReg_number,
                                        orthia::Utf16ToAcp(dir),
                                        numberOfSamples);
    dd::info_out()<<"Control block at: "<<std::hex<<res;
    return 0;
}

void InitMapping()
{
#if defined(_M_AMD64)
    g_RegsMapping[L"rbp"] = 8;
    g_RegsMapping[L"rcx"] = 7;
    g_RegsMapping[L"rdx"] = 6;
    g_RegsMapping[L"r8"] = 5;
    g_RegsMapping[L"r9"] = 4;
    g_RegsMapping[L"rax"] = 3;
    g_RegsMapping[L"r10"] = 2;
    g_RegsMapping[L"r11"] = 1;
#else

#endif
    for(std::map<std::wstring, int>::const_iterator it = g_RegsMapping.begin(), it_end = g_RegsMapping.end();
        it != it_end;
        ++it)
    {
        g_ReverseRegsMapping[it->second] = orthia::Utf16ToAcp(it->first);
    }
    g_ReverseRegsMapping[0] = "none";
}

int wmain(int argc, wchar_t * argv[])
{
    try
    {
        Diana_Init();
        InitMapping();
        if (argc < 2)
        {
            PrintUsage();
            return 0;
        }
        if (wcscmp(argv[1], L"status")==0)
        {
            return StatusCommand(argc, argv);
        }
        else
        if (wcscmp(argv[1], L"selftest")==0)
        {   
            dd::SelfTest();
            return 0;
        }
        else
        if (wcscmp(argv[1], L"inject")==0)
        {   
            return InjectCommand(argc, argv);
        }
        else
            throw std::runtime_error("Unknown command");
    }
    catch(std::exception & e)
    {
        dd::error_out()<<e.what();
        return 1;
    }
    return 0;
}