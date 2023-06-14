#include "orthia_helpers.h"
#include "orthia_model.h"
#include "orthia_sha1.h"
#include "orthia_files.h"

namespace orthia
{

    void OrthiaThrowError(int platformError)
    {
        if (platformError)
        {
            auto errorNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
            auto text = errorNode->QueryValue(ORTHIA_TCSTR("no-app-dir"));
            throw orthia::CWin32Exception(PlatformStringToUtf8(text), platformError);
        }
    }
    std::vector<char> CalcSha1(const std::vector<char>& file)
    {
        std::vector<char> result(SHA1HashSize);
        Sha1Hash((const uint8_t*)file.data(), (unsigned int)file.size(), (uint8_t*)result.data());
        return result;
    }
    std::vector<char> CalcSha1(std::shared_ptr<oui::IFile> file, std::shared_ptr<oui::BaseOperation> completeHandler)
    {
        std::vector<char> result(SHA1HashSize);
        SHA1Context context;
        SHA1Reset(&context);

        int platformError = 0;
        unsigned long long fileSize = 0;
        std::tie(platformError, fileSize) = file->GetSizeInBytes();
        OrthiaThrowError(platformError);

        int pageSize = 1024 * 1024;
        std::vector<char> page(pageSize);
        unsigned long long offset = 0;
        unsigned long long sizeToRead = fileSize;
        for (; sizeToRead;)
        {
            size_t pageToRead = page.size();
            if (sizeToRead < pageToRead)
            {
                pageToRead = (size_t)sizeToRead;
            }
            platformError = file->ReadExact(completeHandler, offset, pageToRead, page);
            OrthiaThrowError(platformError);

            SHA1Input(&context, (const uint8_t *)page.data(), (unsigned int)pageToRead);

            offset = oui::IFile::offset_UseCurrent;
            sizeToRead -= (unsigned long long)pageToRead;
        }
        SHA1Result(&context, (uint8_t*)result.data());
        return result;
    }
    std::vector<char> CalcSha1(CFile &file, std::shared_ptr<oui::BaseOperation> completeHandler)
    {
        std::vector<char> result(SHA1HashSize);
        SHA1Context context;
        SHA1Reset(&context);

        unsigned long long fileSize = file.GetSize();

        file.MoveToFirst(0);
        int pageSize = 1024 * 1024;
        std::vector<char> page(pageSize);
        unsigned long long sizeToRead = fileSize;
        for (; sizeToRead;)
        {
            size_t pageToRead = page.size();
            if (sizeToRead < pageToRead)
            {
                pageToRead = (size_t)sizeToRead;
            }
            file.ExactRead(page.data(), (ULONG)pageToRead);
            SHA1Input(&context, (const uint8_t*)page.data(), (unsigned int)pageToRead);
            sizeToRead -= (unsigned long long)pageToRead;
        }
        SHA1Result(&context, (uint8_t*)result.data());
        return result;
    }

    // CMemoryReaderOverVector
    CMemoryReaderOnLoadedData::CMemoryReaderOnLoadedData(Address_type imageBase, const char* pData, size_t size)
        :
            m_imageBase(imageBase), m_pData(pData), m_size(size)
    {
    }
    void CMemoryReaderOnLoadedData::Read(Address_type offset,
        Address_type bytesToRead,
        void* pBuffer,
        Address_type* pBytesRead,
        int flags,
        Address_type selectorValue,
        DianaUnifiedRegister selectorHint)
    {
        *pBytesRead = 0;
        if (offset < m_imageBase || offset >(m_imageBase + m_size))
        {
            return;
        }
        Address_type relativeOffset = offset - m_imageBase;        
        if (relativeOffset >= m_size)
        {
            return;
        }
        Address_type relativeEnd = relativeOffset;
        DI_CHECK_CPP(Diana_SafeAdd(&relativeEnd, bytesToRead));

        if (relativeEnd >= m_size)
        {
            relativeEnd = m_size;
        }
        Address_type sizeToCopy = relativeEnd - relativeOffset;
        const char* realAddress = m_pData + relativeOffset;
        memcpy(pBuffer, realAddress, (size_t)sizeToCopy);
        *pBytesRead = (size_t)sizeToCopy;
    }
}