#pragma once
#include <windows.h>
#undef max
#undef min

namespace oui
{
    enum class EventType 
    { 
        Auto,
        Manual
    };

    class CEvent:Noncopyable
    {
        HANDLE m_hEvent;
    public:
        CEvent(EventType type)
        {
            m_hEvent = ::CreateEventW(NULL, type == EventType::Manual ? TRUE : FALSE, FALSE, NULL);
            if (m_hEvent == NULL)
                throw std::runtime_error("Can't create event.");
        }
        ~CEvent()
        {
            if (m_hEvent)
                CloseHandle(m_hEvent);
        }

        HANDLE GetHandle()
        {
            return m_hEvent;
        }
        void Set()
        {
            ::SetEvent(m_hEvent);
        }
        void Reset()
        {
            ::ResetEvent(m_hEvent);
        }
        bool Wait(DWORD dwTimeInMs = INFINITE)
        {
            DWORD dwRes = WaitForSingleObject(m_hEvent, dwTimeInMs);
            if (dwRes == WAIT_TIMEOUT)
                return false;
            if (dwRes == WAIT_OBJECT_0)
                return true;
            return false;
        }
    };


    inline std::wstring GetErrorText(int error, DWORD dwLangId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT))
    {
        HLOCAL hlocal = NULL;   // Buffer that gets the error message string

        // Get the error code's textual description
        BOOL fOk = FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            NULL, (DWORD)error, dwLangId,
            (LPWSTR)&hlocal, 0, NULL);

        if (!fOk) {
            // Is it a network-related error?
            HMODULE hDll = LoadLibraryEx(TEXT("netmsg.dll"), NULL,
                DONT_RESOLVE_DLL_REFERENCES);

            if (hDll != NULL) {
                FormatMessageW(
                    FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM,
                    hDll, (DWORD)error, dwLangId,
                    (LPWSTR)&hlocal, 0, NULL);
                FreeLibrary(hDll);
            }
        }

        if (hlocal != NULL)
        {
            std::wstring sErr = (const wchar_t*)LocalLock(hlocal);
            LocalFree(hlocal);
            return sErr;
        }
        else
        {
            return std::to_wstring(error);
        }
    }


    void LogOutput(LogFlags flags, const std::string& text);
    void LogOutput(LogFlags flags, const std::wstring& text);


}