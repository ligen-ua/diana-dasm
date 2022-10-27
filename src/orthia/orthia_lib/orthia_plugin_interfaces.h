#ifndef ORTHIA_PLUGIN_INTERFACES_H
#define ORTHIA_PLUGIN_INTERFACES_H

#include "orthia_interfaces.h"
#include "orthia_utils.h"

namespace orthia
{

class CGenericDebuggerLink
{
    CGenericDebuggerLink(const CGenericDebuggerLink&);
    CGenericDebuggerLink & operator = (const CGenericDebuggerLink&);
    IDebugger * m_pDebugger;
public:
    CGenericDebuggerLink(IDebugger * pDebugger);

    void CheckInterrupt();
    IDebugger * GetDebuggerInterface();
};

class CGenericInterruptChecker
{
    CGenericInterruptChecker(const CGenericInterruptChecker&);
    CGenericInterruptChecker & operator = (const CGenericInterruptChecker&);
    CGenericDebuggerLink m_debugger;
    unsigned int m_rate;
    unsigned int m_currentIndex;
public:
    CGenericInterruptChecker(IDebugger * pDebugger, int rate = 0x100);
    bool CheckInterrupt();
    IDebugger * GetDebuggerInterface();
};

}

#endif
