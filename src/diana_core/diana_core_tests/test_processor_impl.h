#ifndef TEST_PROCESSOR_IMPL_H
#define TEST_PROCESSOR_IMPL_H


extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
#include "diana_processor.h"
#include "diana_allocators.h"
}
#include "test_common.h"


class CTestProcessor
{
    DianaRandomMemoryStream m_memoryStream;
    DianaMAllocator m_memAllocator;
    DianaProcessor m_proc;

    CTestProcessor(const CTestProcessor&);
    CTestProcessor & operator =(const CTestProcessor&);
public:
    CTestProcessor(unsigned char * pBuffer,
                   size_t size,
                   size_t segmentSize = 0,
                   int iMode = DIANA_MODE32)
    {
        Diana_InitRandomMemoryStream(&m_memoryStream,
                                     pBuffer,
                                     size,
                                     segmentSize);

        // memory allocator
        Diana_InitMAllocator(&m_memAllocator);

        // processor
        int res = DianaProcessor_Init(&m_proc, 
                                      &m_memoryStream.parent,
                                      &m_memAllocator.m_parent,
                                      iMode);
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
    }

    ~CTestProcessor()
    {
        DianaProcessor_Free(&m_proc);
    }

    int ExecOnce()
    {
        return DianaProcessor_ExecOnce(&m_proc);
    }

    int Exec(int count)
    {
        for(int i = 0; i < count; ++i)
        {
            int res = DianaProcessor_ExecOnce(&m_proc);
            DIANA_TEST_ASSERT(res == DI_SUCCESS);
            if (res != DI_SUCCESS)
                return res;
        }
        return DI_SUCCESS;
    }

    DianaProcessor * GetSelf()
    {
        return &m_proc;
    }

};



#endif