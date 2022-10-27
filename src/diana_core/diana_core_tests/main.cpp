extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
#include "diana_processor/diana_processor_core.h"
}
#include "test_push.h"
#include "test_mov.h"
#include "test_add.h"
#include "test_cmp.h"
#include "test_imul.h"
#include "test_mov2.h"
#include "test_mov_as.h"
#include "test_mov_spec.h"
#include "test_lea.h"
#include "test_call.h"
#include "test_jmp.h"
#include "test_or.h"
#include "test_suxx.h"
#include "test_integrated.h"
#include "test_x64.h"
#include "test_x64_integrated.h"
#include "test_fpu_mmx32.h"
#include "test_analyze.h"
#include "test_stack.h"
#include "test_new.h"

void test_masm1();
void test_patchers();

int main()
{
    Diana_Init();    
    DianaProcessor_GlobalInit();
    test_new();
    test_stack();
    test_analyze();
    test_integrated2();
    test_fpu_mmx32();
    test_x64();
    test_x64_integrated();
    test_push();
    test_mov();
    test_add();
    test_cmp();
    test_imul();
    test_mov2();
    test_mov_as();
    test_mov_spec();
    test_lea();
    test_call();
    test_jmp();
    test_or();
    test_suxx();
    test_integrated();
    test_masm1();
    test_patchers();
    return 0;
}
