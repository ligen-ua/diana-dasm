#include "cmd_common.h"

namespace orthia
{
    // CTextToolOutputStream
    void CTextToolOutputStream::OutVar(const std::string& name, const std::string& value)
    {
        std::stringstream tmp;
        tmp << name << ": " << value << "\n";
        std::wcout<<orthia::Utf8ToUtf16(tmp.str());
    }
    void CTextToolOutputStream::OutVar(const std::wstring& name, const std::wstring& value)
    {
        std::wcout << name << ": " << value<<"\n";
    }
    void CTextToolOutputStream::FinalFlush()
    {
        std::wcout.flush();
    }
        
    void CJSONToolOutputStream::OutVar(const std::string& name, const std::string& value)
    {
        m_jsonObject.set(name, value);
    }
    void CJSONToolOutputStream::OutVar(const std::wstring& name, const std::wstring& value)
    {
        m_jsonObject.set(orthia::Utf16ToUtf8(name), orthia::Utf16ToUtf8(value));
    }    
    void CJSONToolOutputStream::FinalFlush()
    {
        std::wcout << orthia::Utf8ToUtf16(m_jsonObject.serialize());
        std::wcout.flush();
    }
    
    void PrintUsage()
    {
        std::wcout << L"Usage: 1) dump <module> <functions> [--fmt <json>] [--base <imagebase>] [--pdb <pdbfile>]\n";
    }
    int PrintInvalidArgument(const wchar_t* arg, const wchar_t* expect)
    {
        std::wcerr<< L"Invalid argument: "<<arg<<"\n";
        if (expect)
        {
            std::wcerr << expect << "\n";
        }
        return 1;
    }
    int ValidateArgument(const wchar_t* arg, const wchar_t* expect)
    {
        if (wcsncmp(arg, L"--", 2) == 0)
        {
            return PrintInvalidArgument(arg, expect);
        }
        return 0;
    }

    DI_UINT64 CaptureArgument64(const std::wstring& arg)
    {
        if (arg.empty())
        {
            throw std::runtime_error("Invalid argument");
        }
        DI_UINT64 result = 0;

        if (arg.find('`') != arg.npos)
        {
            // windbg uses vars like fffff801`11c00000 
            // assume it is hex
            std::wstring tmp = arg;
            tmp.erase(std::remove(tmp.begin(), tmp.end(), '`'), tmp.end());

            if (tmp.empty())
            {
                throw std::runtime_error("Invalid argument");
            }

            // it still can have prefix
            if (tmp[0] == '0' && tmp[0] == 'x')
            {
                orthia::HexStringToObject(std::wstring(tmp.begin() + 2, tmp.end()), &result);
            }
            else
            {
                orthia::HexStringToObject(tmp, &result);
            }
            return result;
        }
        if (arg.size() > 2)
        {
            if (arg[0] == '0' && arg[0] == 'x')
            {
                orthia::HexStringToObject(std::wstring(arg.begin() + 2, arg.end()), &result);
                return result;
            }
            if (arg[0] == '0' && arg[0] == 'n')
            {
                orthia::StringToObject(std::wstring(arg.begin() + 2, arg.end()), &result);
                return result;
            }
        }
        orthia::StringToObject(arg, &result);
        return result;
    }

}
