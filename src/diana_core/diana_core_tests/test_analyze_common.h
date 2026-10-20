#ifndef TEST_ANALYZE_COMMON_H
#define TEST_ANALYZE_COMMON_H

// the code analyzer over a buffer, shared by the x86 (test_analyze.cpp) and AArch64 (test_analyze_arm64.cpp) tests
#include "test_common.h"
extern "C"
{
#include "diana_analyze.h"
}

struct TestStream:public DianaMovableReadStream
{
    OPERAND_SIZE m_base;
    OPERAND_SIZE m_current;
    OPERAND_SIZE m_size;

    TestStream(OPERAND_SIZE base,
               OPERAND_SIZE current,
               OPERAND_SIZE size)
         : 
            m_base(base), 
            m_current(current),
            m_size(size)
    {
    }
};
struct TestAnalyzeEnvironment
{
    DianaAnalyzeObserver observer; // must be first
    TestStream stream;
    OPERAND_SIZE imageBase;        // absolute addresses are imageBase + offset
    OPERAND_SIZE minimalAddress;   // offsets below are out of the module (the image doesn't start at 0)

    TestAnalyzeEnvironment(OPERAND_SIZE base,
                           OPERAND_SIZE current,
                           OPERAND_SIZE size);
};

struct IntructionInfo
{
    int offset;
    char * pXrefsTo;
    char * pXrefsFrom;
};

// checks the xrefs of a list: "29;$192;#50", offsets in order; $ an external target, # a data xref.
// 0: every xref of the list is invalid
void VerifyREF(const char * pTestRefs,
               const Diana_FList_List * pRefs,
               int index);

// the xref from the instruction to the offset with these flags, 0 if there is none
Diana_XRef * FindXRefFrom(Diana_Instruction * pInstruction, OPERAND_SIZE target, int flags);

// analyzes a buffer from offset 0 (or start) with Diana_AnalyzeCodeEx2
struct AnalyzedCode
{
    TestAnalyzeEnvironment env;
    Diana_InstructionsOwner owner;
    AnalyzedCode(unsigned char * pCode, size_t size, int mode,
                 const DianaAnalyzeParams * pParams = 0, int flags = 0, size_t start = 0);
    ~AnalyzedCode();
    // the valid instruction at the offset, 0 if there is none
    Diana_Instruction * At(OPERAND_SIZE offset);
    // then Diana_AnalyzeLandingPads over the whole buffer
    void AnalyzeLandingPads(int mode, const DianaAnalyzeParams * pParams);
};

#endif
