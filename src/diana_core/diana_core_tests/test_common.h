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

#if _DEBUG
#define DIANA_TEST_ASSERT_IMPL(X, Y)  if (!(X))   {   std::cout<<"[ERROR] \""<<#X<<"\" failed in \""<<__FILE__<<"\" at line "<<__LINE__<<"\n";   Diana_FatalBreak();  Y; }
#else
#define DIANA_TEST_ASSERT_IMPL(X, Y)  if (!(X))   {   std::cout<<"[ERROR] \""<<#X<<"\" failed in \""<<__FILE__<<"\" at line "<<__LINE__<<"\n";  Y; }
#endif

#define DIANA_TEST(X) try{ std::cout<<"[TEST: " #X "]\n"; X; }catch(DianaTestError & ){ std::cout<<"[ERROR] Test failed: "<<#X<<"\n\n"; }catch(const std::exception & e) {std::cout<<"[ERROR] Test failed: "<<#X<<", exception: \""<<e.what()<<"\"\n\n"; }
#define DIANA_TEST_ASSERT(X)  DIANA_TEST_ASSERT_IMPL(X, throw DianaTestError());
#define DIANA_TEST_ASSERT_IF(X)  DIANA_TEST_ASSERT_IMPL(X, ;) else
#define DIANA_TEST_VAR(X) try{ DIANA_TEST_ASSERT(X) }catch(DianaTestError & ){ std::cout<<"[ERROR] Test failed: "<<#X<<"\n\n"; }catch(const std::exception & e) {std::cout<<"[ERROR] Test failed: "<<#X<<", exception: \""<<e.what()<<"\"\n\n"; }


#define DIANA_TEST_EXCEPTION(X, EXC)  try { X; DIANA_TEST_ASSERT((#X)?FALSE:FALSE); } catch(EXC) {  }
#define DIANA_TEST_EXCEPTION2(X, EXC)  try { X; DIANA_TEST_ASSERT((#X)?FALSE:FALSE); } catch(EXC) 

int Diana_ParseCmdOnBuffer_test(int iMode,
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


#endif