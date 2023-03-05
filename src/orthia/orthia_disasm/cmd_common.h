#pragma once
struct IUnknown;

#include <iostream>
#include <string>
#include "orthia_utils.h"
#include <optional>
#include "orthia_json.h"

namespace orthia
{

    struct IToolOutputStream
    {
        virtual ~IToolOutputStream() {}
        virtual void OutVar(const std::string& name, const std::string& value) = 0;
        virtual void OutVar(const std::wstring& name, const std::wstring& value) = 0;
        virtual void FinalFlush() = 0;

        struct FinalFlusher
        {
            typedef IToolOutputStream* ObjectType;
            static void Free(IToolOutputStream* pObject)
            {
                pObject->FinalFlush();
            }
        };
    };
    class CTextToolOutputStream:public IToolOutputStream
    {
    public:
        void OutVar(const std::string& name, const std::string& value) override;
        void OutVar(const std::wstring& name, const std::wstring& value) override;
        void FinalFlush() override;
    };
    class CJSONToolOutputStream :public IToolOutputStream
    {
        JSONObject m_jsonObject;
    public:
        void OutVar(const std::string& name, const std::string& value) override;
        void OutVar(const std::wstring& name, const std::wstring& value) override;
        void FinalFlush() override;
    };
    int ParseAndRunDump(int argc, wchar_t* argv[]);
    void PrintUsage();
    
    int PrintInvalidArgument(const wchar_t* arg, const wchar_t* expect = 0);
    int ValidateArgument(const wchar_t* arg, const wchar_t* expect = 0);

    DI_UINT64 CaptureArgument64(const std::wstring& arg);
}

