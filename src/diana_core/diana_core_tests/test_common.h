#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <iostream>
extern "C"
{
#include "diana_core.h"
}
#include "diana_pe_cpp.h"
#include "string.h"
struct DianaTestError:public std::runtime_error
{
    DianaTestError()
        :
            std::runtime_error("DianaTestError")
    {
    }
    virtual ~DianaTestError() {}
};

extern int g_diana_resultCode;

// every failed assertion fails the run, also the ones that don't throw (DIANA_TEST_ASSERT_IF)
#if _DEBUG
#define DIANA_TEST_ASSERT_IMPL(X, Y)  if (!(X))   {   std::cout<<"[ERROR] \""<<#X<<"\" failed in \""<<__FILE__<<"\" at line "<<__LINE__<<"\n";   g_diana_resultCode = 1;   Diana_FatalBreak();  Y; }
#else
#define DIANA_TEST_ASSERT_IMPL(X, Y)  if (!(X))   {   std::cout<<"[ERROR] \""<<#X<<"\" failed in \""<<__FILE__<<"\" at line "<<__LINE__<<"\n";   g_diana_resultCode = 1;  Y; }
#endif
#define DIANA_TEST(X) try{ std::cout<<"[TEST: " #X "]\n"; X; }catch(DianaTestError & ){ std::cout<<"[ERROR] Test failed: "<<#X<<"\n\n"; g_diana_resultCode = 1; }catch(const std::exception & e) {std::cout<<"[ERROR] Test failed: "<<#X<<", exception: \""<<e.what()<<"\"\n\n"; g_diana_resultCode = 1; }
#define DIANA_TEST_ASSERT(X)  DIANA_TEST_ASSERT_IMPL(X, throw DianaTestError());
#define DIANA_TEST_ASSERT_IF(X)  DIANA_TEST_ASSERT_IMPL(X, ;) else
#define DIANA_TEST_VAR(X) try{ DIANA_TEST_ASSERT(X) }catch(DianaTestError & ){ std::cout<<"[ERROR] Test failed: "<<#X<<"\n\n"; }catch(const std::exception & e) {std::cout<<"[ERROR] Test failed: "<<#X<<", exception: \""<<e.what()<<"\"\n\n"; }


#define DIANA_TEST_EXCEPTION(X, EXC)  try { X; DIANA_TEST_ASSERT((#X)?0:0); } catch(EXC) {  }
#define DIANA_TEST_EXCEPTION2(X, EXC)  try { X; DIANA_TEST_ASSERT((#X)?0:0); } catch(EXC) 

// Diana_ParseCmdOnBuffer_test and Diana_ParseCmdOnBufferEx_test return this when the instruction doesn't
// take the whole buffer
#define DIANA_TEST_ERROR_SIZE  (-1000)

int Diana_ParseCmdOnBuffer_test(int iMode,
                           void * pBuffer,
                           size_t size,
                           DianaBaseGenObject_type * pInitialLine,  // IN
                           DianaParserResult * pResult,  //OUT
                           size_t * sizeRead);    // OUT

// Diana_ParseCmdOnBuffer_test for any architecture, iArch is DIANA_ARCH_*
int Diana_ParseCmdOnBufferEx_test(int iMode,
                                  int iArch,
                                  void * pBuffer,
                                  size_t size,
                                  DianaBaseGenObject_type * pInitialLine,  // IN
                                  DianaParserResult * pResult,  //OUT
                                  size_t * sizeRead);    // OUT

struct TestEntry_Name
{
    const char * pCmdName;
    int iCmdSize;
    int iPrivileged;
};

#ifdef WIN32
#define DIANA_TEST_WIN32(X)  DIANA_TEST(X)
#else
#define DIANA_TEST_WIN32(X) 
#endif

#endif