#include "orthia_process_adapter.h"

namespace orthia
{
    ProcessReaderAdapter::ProcessReaderAdapter(oui::IProcess* process)
        :
            m_process(process)
    {
    }
    void ProcessReaderAdapter::Read(Address_type offset,
        Address_type bytesToRead,
        void* pBuffer,
        Address_type* pBytesRead,
        int flags,
        Address_type selectorValue,
        DianaUnifiedRegister selectorHint)
    {
        std::vector<char> data;
        *pBytesRead = 0;
        if (!m_process->ReadExactEx2(offset, pBuffer, (size_t)bytesToRead))
        {
            *pBytesRead = bytesToRead;
        }
    }
}
