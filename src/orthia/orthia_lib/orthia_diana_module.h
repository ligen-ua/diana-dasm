#ifndef ORTHIA_DIANA_MODULE_H
#define ORTHIA_DIANA_MODULE_H

#include "orthia_interfaces.h"

namespace orthia
{


class CDianaModule;
class CDianaInstructionIterator
{
    int m_currentInstruction;
    CDianaModule * m_pModule;
    void SkipExternals();
public:
    CDianaInstructionIterator();
    void Init(CDianaModule * pModule);
    void MoveToFirst();
    void MoveToNext();
    bool IsEmpty() const;
    Address_type GetInstructionOffset();
    void QueryRefsToCurrentInstuction(std::vector<CommonReferenceInfo> * pInfo);
    void QueryRefsFromCurrentInstruction(std::vector<CommonReferenceInfo> * pInfo);
};

class CDianaModuleImpl;
class CDianaModule
{
    friend class CDianaInstructionIterator;
    Address_type m_offset;
    IMemoryReader * m_pMemoryReader;
    std::auto_ptr<CDianaModuleImpl> m_impl;
public:
    CDianaModule();
    ~CDianaModule();
    void Init(Address_type offset,
              IMemoryReader * pMemoryReader);

    void InitRaw(Address_type offset,
                 Address_type size,
                 IMemoryReader * pMemoryReader,
                 int mode);

    Address_type GetModuleAddress() const;
    Address_type GetModuleSize() const;
    void Analyze(int analyserFlags);
    std::wstring GetName() const;

    void QueryInstructionIterator(CDianaInstructionIterator * pIterator);
};

}
#endif
