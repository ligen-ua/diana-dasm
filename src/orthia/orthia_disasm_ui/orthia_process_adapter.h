#pragma once

#include "orthia_interfaces.h"
#include "oui_processes.h"

namespace orthia
{
    class ProcessReaderAdapter:public IMemoryReader
    {
        oui::IProcess* m_process;
    public:
        ProcessReaderAdapter(oui::IProcess* process);
        void Read(Address_type offset,
            Address_type bytesToRead,
            void* pBuffer,
            Address_type* pBytesRead,
            int flags,
            Address_type selectorValue,
            DianaUnifiedRegister selectorHint) override;
    };
}
