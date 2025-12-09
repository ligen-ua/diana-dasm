#pragma once
#include "orthia_utils.h"

namespace orthia
{

    class CResource
    {
        void* m_pData;
        size_t m_size;

    public:
        CResource()
            :
            m_pData(0),
            m_size(0)
        {
        }

        CResource(HMODULE hModule,
            unsigned int id,
            const ORTHIA_TCHAR* pType)
            :
            m_pData(0),
            m_size(0)
        {
            Load(hModule, id, pType);
        }

        ~CResource()
        {
        }

        void Load(HMODULE hModule,
            unsigned int id,
            const ORTHIA_TCHAR* pType)
        {
            HRSRC hResource = FindResourceW(hModule, MAKEINTRESOURCEW(id), pType);
            if (hResource == NULL)
            {
                DWORD dwErr = GetLastError();
                throw orthia::CWin32Exception("Can't find resource", dwErr);
            }

            HGLOBAL hTable = LoadResource(hModule, hResource);
            if (!hTable)
            {
                DWORD dwErr = GetLastError();
                throw orthia::CWin32Exception("Can't load resource", dwErr);
            }

            m_pData = LockResource(hTable);
            if (!m_pData)
            {
                DWORD dwErr = GetLastError();
                throw orthia::CWin32Exception("Can't lock resource", dwErr);
            }

            m_size = SizeofResource(hModule, hResource);
            if (!m_size)
            {
                m_pData = 0;
                DWORD dwErr = GetLastError();
                throw orthia::CWin32Exception("Can't get size of resource", dwErr);
            }
        }

        size_t size() const
        {
            return m_size;
        }
        const void * data() const
        {
            return m_pData;
        }
        const char* begin() const
        {
            return (char*)m_pData;
        }
        const char* end() const
        {
            return (char*)m_pData + size();
        }
    };

}