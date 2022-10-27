#pragma once

#include "test_common.h"
#include "orthia_module_manager.h"
#include "orthia_interfaces.h"
#include "orthia_database_module.h"

// wcscat
#pragma warning(disable:4996)

struct OT_TestEnv
{
    orthia::CModuleManager manager;
    orthia::CMemoryReader reader;

    OT_TestEnv(bool force = true)
    {
        std::vector<wchar_t> buf(1024);
        GetTempPath((DWORD)buf.size(), &buf.front());
        wcscat(&buf.front(), L"\\orthia_test");
        CreateDirectory(&buf.front(), 0);
        wcscat(&buf.front(), L"\\test.db");
        manager.Reinit(&buf.front(), force);
    }
    orthia::intrusive_ptr<orthia::CDatabaseManager> GetDatabaseManager()
    {
        return manager.QueryDatabaseManager();
    }
};

class CTestDataGenerator
{
    struct pcg32_random
    { 
        unsigned long long state;  
        unsigned long long inc;

        pcg32_random()
            :
                state(0), inc(0)
        {
        }
    };
    pcg32_random m_random;

    unsigned int pcg32_random_r(pcg32_random * rng)
    {
        unsigned long long oldstate = rng->state;
        // Advance internal state
        rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
        // Calculate output function (XSH RR), uses old state for max ILP
        unsigned int xorshifted = (unsigned int)(((oldstate >> 18u) ^ oldstate) >> 27u);
        unsigned int rot = (unsigned int)(oldstate >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((0-rot) & 31));
    }

public:
    CTestDataGenerator()
    {
        m_random.state = 0x882726112133;
    }
    void GenerateTestData(std::vector<char> & test, int size)
    {
        test.resize(size);
        if (!size)
        {
            return;
        }
        char * pDestData = &test.front();
        char * pDestDataEnd = pDestData + size;
        int intsCount = size/sizeof(unsigned int);

        unsigned int * pDestDataUint = (unsigned int * )&test.front();
        for(int i = 0; i < intsCount; ++i, ++pDestDataUint)
        {
            *pDestDataUint = pcg32_random_r(&m_random);
        }

        for(char * p = (char * )pDestDataUint; p != pDestDataEnd; ++p)
        {
            *p = (char)pcg32_random_r(&m_random);
        }
    }
};


class CLinkObserverOverWin32:public diana::CBasePeLinkImportsObserver, public orthia::IAPIHandlerDebugInterface
{
    CLinkObserverOverWin32(const CLinkObserverOverWin32 & );
    CLinkObserverOverWin32 & operator = (const CLinkObserverOverWin32 & );
protected:
    class CLibrary
    {
        HMODULE m_hmodule;
    public:
        CLibrary()
            :
                m_hmodule(0)
        {
        }
        CLibrary(const CLibrary & other)
            :
                m_hmodule(0)
        {
            if (other.m_hmodule)
            {
                throw std::runtime_error("Internal error");
            }
        }
        CLibrary & operator = (const CLibrary & other)
        {
            if (other.m_hmodule || m_hmodule)
            {
                throw std::runtime_error("Internal error");
            }
            return *this;
        }
        ~CLibrary()
        {
            if (m_hmodule)
            {
                FreeLibrary(m_hmodule);
            }
        }
        HMODULE GetModule() 
        {
            return m_hmodule;
        }
        void Init(const char * pStr)
        {
            m_hmodule = LoadLibraryA(pStr);
            if (!m_hmodule)
            {
                throw std::runtime_error(std::string("Can't load library: ") + pStr);
            }
        }

    };
    typedef std::map<std::string, CLibrary> LibrariesMap_type;
    LibrariesMap_type m_librariesMap;
public:
    CLinkObserverOverWin32()
    {
    }
    virtual void Init(orthia::CMemoryStorageOfModifiedData * )
    {
    }
    virtual void Print(const std::wstring & text)
    {
        std::wcout<<text;
    }
    CLibrary * QueryDll(const char * pStr)
    {
        std::string dllKey(orthia::Downcase_Ansi(pStr));
        std::pair<LibrariesMap_type::iterator, bool> res = m_librariesMap.insert(std::make_pair(dllKey, CLibrary()));
        if (res.second)
        {
            res.first->second.Init(pStr);
        }
        return &res.first->second;
    }
    void CheckAddress(OPERAND_SIZE address)
    {
        if (!address)
        {
            throw std::runtime_error(std::string("Can't find function"));
        }
    }
    virtual Debuggee_type GetDebuggeeType()
    {
        return dtUser;
    }
    virtual void QueryFunctionByOrdinal(const char * pDllName,
                                        DI_UINT32 ordinal,
                                        OPERAND_SIZE * pAddress)
    {
        HMODULE hModule = QueryDll(pDllName)->GetModule();
        *pAddress = (OPERAND_SIZE)GetProcAddress(hModule, MAKEINTRESOURCEA(ordinal));
        CheckAddress(*pAddress);
    }
    virtual void QueryFunctionByName(const char * pDllName,
                                     const char * pFunctionName,
                                     DI_UINT32 hint,
                                     OPERAND_SIZE * pAddress)
    {
        HMODULE hModule = QueryDll(pDllName)->GetModule();
        *pAddress = (OPERAND_SIZE)GetProcAddress(hModule, pFunctionName);
        CheckAddress(*pAddress);
    }

    virtual OPERAND_SIZE QueryModule(const char * pDllName)
    {
        std::string dllKey(orthia::Downcase_Ansi(pDllName));
        LibrariesMap_type::const_iterator it = m_librariesMap.find(dllKey);
        if (it == m_librariesMap.end())
        {
            HMODULE hMod = GetModuleHandleA(pDllName);
            if (!hMod)
            {
                return 0;
            }
        }
        return (OPERAND_SIZE)(QueryDll(pDllName)->GetModule());
    }
    virtual OPERAND_SIZE QueryFunctionAddress(OPERAND_SIZE module,
                                              const char * pDllName, 
                                              const char * pFunctionName)
    {
        if (!GetModuleHandleA(pDllName))
        {
            return 0;
        }
        OPERAND_SIZE res = 0;
        QueryFunctionByName(pDllName,
                            pFunctionName,
                            0,
                            &res);
        return res;
    }
};
