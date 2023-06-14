#pragma once
#include "orthia_utils.h"
#include "oui_filesystem.h"
#include "orthia_interfaces.h"

namespace orthia
{

    class CMemoryReaderOnLoadedData:public IMemoryReader
    {
        Address_type m_imageBase;
        const char* m_pData;
        size_t m_size;
    public:
        CMemoryReaderOnLoadedData(Address_type imageBase, const char * pData, size_t size);
        virtual void Read(Address_type offset,
            Address_type bytesToRead,
            void* pBuffer,
            Address_type* pBytesRead,
            int flags,
            Address_type selectorValue,
            DianaUnifiedRegister selectorHint);
    };

    std::vector<char> CalcSha1(const std::vector<char>& file);
    std::vector<char> CalcSha1(std::shared_ptr<oui::IFile> file, std::shared_ptr<oui::BaseOperation> completeHandler);

    class CFile;
    std::vector<char> CalcSha1(CFile& file, std::shared_ptr<oui::BaseOperation> completeHandler);


}