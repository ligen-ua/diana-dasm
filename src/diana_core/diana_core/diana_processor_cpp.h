#ifndef DIANA_PROCESSOR_CPP_H
#define DIANA_PROCESSOR_CPP_H

#include "diana_core_cpp.h"
extern "C"
{
#include "diana_processor.h"
#include "diana_streams.h"
#include "diana_gen.h"
}

namespace diana
{

int DianaCPP_CallStackProxyFnc(struct _dianaProcessor * pProcessor,
                               void * pCustomContext,
                               OPERAND_SIZE retAddress,
                               int * pbContinue);
class CBaseProcessor
{
    CBaseProcessor(const CBaseProcessor&);
    CBaseProcessor & operator =(const CBaseProcessor&);

protected:
    int m_mode;
    DianaProcessor m_processor;
    DIANA_UUID m_zeroUUID;
    CDefaultAllocator m_defaultAllocator;

    virtual void InitProcessor() = 0;
public:
    CBaseProcessor(int mode)
        :
            m_mode(mode)
    {
        memset(&m_processor, 0, sizeof(m_processor));
        memset(&m_zeroUUID, 0, sizeof(m_zeroUUID));
    }
    void Init()
    {
        InitProcessor();
    }
    virtual ~CBaseProcessor()
    {
        if (DIANA_UUID_Compare(&m_processor.m_base.m_id, &m_zeroUUID)!=0)
        {
            DianaProcessor_Free(&m_processor);
            memset(&m_processor, 0, sizeof(m_processor));
        }
    }

    void SetOptions(int optionsToSet, int optionsToRemove)
    {
        DianaProcessor_SetOptions(&m_processor, optionsToSet, optionsToRemove);
    }
    int ExecOnce()
    {
        return DianaProcessor_ExecOnce(&m_processor);
    }

    int Exec(int count)
    {
        for(int i = 0; i < count; ++i)
        {
            int res = DianaProcessor_ExecOnce(&m_processor);
            if (res != DI_SUCCESS)
                return res;
        }
        return DI_SUCCESS;
    }

// Uncomment this for debug
//#define DI_PROC_COLLECT_DEBUG_INFORMATION

    template<class CheckerType>
    int Exec(long long commandsCount, 
             CheckerType * pChecker,
             long long * pCommandsCount = 0)
    {
        if (commandsCount == -1)
        {
            commandsCount = LLONG_MAX;
        }
        long long outCommandsCount = 0;
        if (!pCommandsCount)
        {
            pCommandsCount = &outCommandsCount;
        }
        *pCommandsCount = 0;
        DianaProcessor * pCallContext =  &m_processor;

#ifdef DI_PROC_COLLECT_DEBUG_INFORMATION
        int ripsCount = 10240;
        std::vector<Diana_Processor_Registers_Context> ripsBuffer(ripsCount);
        int ripOffset = 0;
#endif
        for(long long i = 0; i < commandsCount; ++i, ++*pCommandsCount)
        {
            pChecker->CheckInterrupt();
            OPERAND_SIZE rip = GET_REG_RIP;
            int res = DianaProcessor_ExecOnce(&m_processor);

#ifdef DI_PROC_COLLECT_DEBUG_INFORMATION
            Diana_Processor_Registers_Context �ontext;            
            DianaProcessor_QueryContext(&m_processor, &�ontext);
            ripsBuffer[ripOffset] = �ontext;
            if (res != DI_SUCCESS)
            {
                void * pArray = &ripsBuffer[ripOffset];
                &pArray;
                __debugbreak();
                return res;
            }            
            if (++ripOffset >= ripsCount)
            {
                ripOffset = 0;
            }
#else
            if (res != DI_SUCCESS)
            {
                return res;
            }
#endif
        }
        return DI_SUCCESS;
    }

    struct StackProxyContext
    {
        std::vector<OPERAND_SIZE> * pCallStack;
        int count;
    };
    void QueryCallStack(std::vector<OPERAND_SIZE> * pCallStack, int count)
    {
        pCallStack->clear();

        StackProxyContext context;
        context.pCallStack = pCallStack;
        context.count = count;

        DianaProcessor_QueryCurrentStack(&m_processor,
                                         DianaCPP_CallStackProxyFnc,
                                         &context);
    }
    DianaProcessor * GetSelf()
    {
        return &m_processor;
    }
};

inline int DianaCPP_CallStackProxyFnc(struct _dianaProcessor * pProcessor,
                                      void * pCustomContext,
                                      OPERAND_SIZE retAddress,
                                      int * pbContinue)
{

    DI_CPP_BEGIN    
        
        CBaseProcessor::StackProxyContext * pContext = (CBaseProcessor::StackProxyContext * )pCustomContext;
        pContext->pCallStack->push_back(retAddress);
        if ((int)pContext->pCallStack->size() > pContext->count)
        {
            *pbContinue = 0;
        }
        else
        {
            *pbContinue = 1;
        }

    DI_CPP_END 
}

}
#endif