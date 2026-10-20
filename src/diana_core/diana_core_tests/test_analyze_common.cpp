#include "test_analyze_common.h"

static
int DianaRead(void * pThis, 
              void * pBuffer, 
              int iBufferSize, 
              int * readed)
{
    TestStream * pStream = (TestStream * )pThis;
    size_t data = (size_t)(pStream->m_base + pStream->m_current);

    if (pStream->m_current+iBufferSize > pStream->m_size)
    {
        iBufferSize = (int)(pStream->m_size - pStream->m_current);
        if (iBufferSize <=0)
            return DI_END;
    }
    memcpy(pBuffer, (void*)data, iBufferSize);
    *readed = iBufferSize;
    pStream->m_current += iBufferSize;
    return DI_SUCCESS;
}

static
int DianaAnalyzeMoveTo(void * pThis, 
                       OPERAND_SIZE offset)
{
    TestStream * pStream = (TestStream * )pThis;
    pStream->m_current = offset;
    return DI_SUCCESS;
}
static 
int DianaAnalyzeRandomRead(void * pThis, 
                       OPERAND_SIZE offset,
                       void * pBuffer, 
                       int iBufferSize, 
                       OPERAND_SIZE * readBytes,
                       int flags)
{
    TestStream * pStream = (TestStream * )pThis;
    int sizeToGive = 0;
    
    if (flags & DIANA_ANALYZE_RANDOM_READ_ABSOLUTE)
    {
        return DI_END_OF_STREAM;
    }
    if (offset >= pStream->m_size)
    {
        return DI_END_OF_STREAM;
    }

    sizeToGive = (int)(pStream->m_size - offset);
    if (sizeToGive > iBufferSize)
        sizeToGive = iBufferSize;

    memcpy(pBuffer,(char*)pStream->m_base+offset, sizeToGive);
    *readBytes = sizeToGive;
    return 0;
}
 
static
int DianaAnalyzeJumpAddress(void * pThis, 
                           OPERAND_SIZE address,
                           int flags,
                           OPERAND_SIZE * pRelativeOffset,
                           DianaAnalyzeAddressResult_type * pResult)
{
    TestAnalyzeEnvironment * pObserver = (TestAnalyzeEnvironment * )pThis;

    if (flags & DIANA_ANALYZE_ABSOLUTE_ADDRESS)
        address -= pObserver->imageBase;
    *pRelativeOffset = address;
    *pResult = diaJumpNormal;
    if (address >= pObserver->stream.m_size || address < pObserver->minimalAddress)
    {
        *pResult = diaJumpExternal;
    }
    return DI_SUCCESS;
}

void VerifyREF(const char * pTestRefs, 
               const Diana_FList_List * pRefs,
               int index)
{
    bool bEnd = false;
    const char * pLast = pTestRefs;
    Diana_XRef * pCurXRef = 0;
    Diana_SubXRef * pSubRef = (Diana_SubXRef * )Diana_FList_First(pRefs);
    
    if (pTestRefs)
    {
        for(int u = 0; !bEnd; ++u)
        {
            DIANA_TEST_ASSERT(pSubRef);
            pCurXRef = Diana_CastXREF(&pSubRef->m_instructionEntry, index);

            char ch = pTestRefs[u];
            switch(ch)
            {
            default:
                continue;
            case 0:
                bEnd = true;
            case ';':;
            }
            bool bExtern = false;
            bool bData = false;
            if (pLast[0] == '$')
            {
                bExtern = true;
                ++pLast;
            }
            // #: a data xref, the reference is kept although the target is not code
            if (pLast[0] == '#')
            {
                bData = true;
                ++pLast;
            }
            int value = atoi(pLast);

            DIANA_TEST_ASSERT(pCurXRef);
            if (!pCurXRef)
                return;

            DIANA_TEST_ASSERT(pCurXRef->m_subrefs[index].m_pInstruction->m_offset == value);
            if (bData)
            {
                DIANA_TEST_ASSERT((pCurXRef->m_flags & (DI_XREF_DATA|DI_XREF_INVALID)) == DI_XREF_DATA);
            }

            pSubRef = (Diana_SubXRef *)Diana_FList_Next(pRefs, &pSubRef->m_instructionEntry);
            pLast = pTestRefs + u + 1;
        }
    }
    else
    {
        while(pSubRef)
        {
            pCurXRef = Diana_CastXREF(&pSubRef->m_instructionEntry, index);

            DIANA_TEST_ASSERT(pCurXRef->m_flags & DI_XREF_INVALID);

            pSubRef = (Diana_SubXRef *)Diana_FList_Next(pRefs, &pSubRef->m_instructionEntry);
        }
    }
    
}

TestAnalyzeEnvironment::TestAnalyzeEnvironment(OPERAND_SIZE base,
                                               OPERAND_SIZE current,
                                               OPERAND_SIZE size)
        :
            stream(base, current, size),
            imageBase(0),
            minimalAddress(0)
{
    DianaMovableReadStream_Init(&stream,
                                DianaRead,
                                DianaAnalyzeMoveTo,
                                DianaAnalyzeRandomRead);
    DianaAnalyzeObserver_Init(&observer, 
                          &stream,
                          DianaAnalyzeJumpAddress);
}

// the xref from the instruction to the offset with these flags, 0 if there is none
Diana_XRef * FindXRefFrom(Diana_Instruction * pInstruction, OPERAND_SIZE target, int flags)
{
    if (!pInstruction)
        return 0;
    const Diana_FList_List * pRefs = &pInstruction->m_referencesFromThisInstruction;
    for (Diana_SubXRef * pSubRef = (Diana_SubXRef *)Diana_FList_First(pRefs);
         pSubRef;
         pSubRef = (Diana_SubXRef *)Diana_FList_Next(pRefs, &pSubRef->m_instructionEntry))
    {
        Diana_XRef * pXRef = Diana_CastXREF(&pSubRef->m_instructionEntry, 1);
        if (pXRef->m_subrefs[1].m_pInstruction->m_offset == target && (pXRef->m_flags & flags) == flags)
            return pXRef;
    }
    return 0;
}

AnalyzedCode::AnalyzedCode(unsigned char * pCode, size_t size, int mode,
                           const DianaAnalyzeParams * pParams, int flags, size_t start)
    : env((OPERAND_SIZE)pCode, 0, size)
{
    if (pParams)
        env.imageBase = pParams->imageBase;
    Diana_Stack stack;
    DIANA_TEST_ASSERT(DI_SUCCESS == Diana_InstructionsOwner_Init(&owner, size, 0));
    DIANA_TEST_ASSERT(DI_SUCCESS == Diana_Stack_Init(&stack, 4096, sizeof(Diana_RouteInfo)));
    int status = Diana_AnalyzeCodeEx2(&owner, &env.observer, mode, start, size, &stack, flags, pParams);
    Diana_Stack_Free(&stack);
    DIANA_TEST_ASSERT(DI_SUCCESS == status);
}

void AnalyzedCode::AnalyzeLandingPads(int mode, const DianaAnalyzeParams * pParams)
{
    Diana_Stack stack;
    DIANA_TEST_ASSERT(DI_SUCCESS == Diana_Stack_Init(&stack, 4096, sizeof(Diana_RouteInfo)));
    int status = Diana_AnalyzeLandingPads(&owner, &env.observer, mode, 0, env.stream.m_size, env.stream.m_size,
                                          &stack, pParams);
    Diana_Stack_Free(&stack);
    DIANA_TEST_ASSERT(DI_SUCCESS == status);
}

AnalyzedCode::~AnalyzedCode()
{
    Diana_InstructionsOwner_Free(&owner);
}

Diana_Instruction * AnalyzedCode::At(OPERAND_SIZE offset)
{
    Diana_Instruction * pInstruction = owner.m_ppPresenceVec[offset];
    return pInstruction && !(pInstruction->m_flags & DI_INSTRUCTION_INVALID) ? pInstruction : 0;
}
