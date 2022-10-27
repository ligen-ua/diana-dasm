extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
#include "diana_processor.h"
}
#include "test_processor.h"
#include "test_processor_flags.h"
#include "test_processor_call.h"
#include "test_processor_c.h"
#include "test_processor_d.h"
#include "test_processor_i.h"
#include "test_processor_j.h"
#include "test_processor_l.h"
#include "test_processor_m.h"
#include "test_processor_r.h"
#include "test_processor_s.h"
#include "test_processor_a.h"
#include "test_processor_x.h"
#include "test_processor_o.h"
#include "test_processor_n.h"
#include "test_processor_b.h"
#include "test_processor_m_xmm.h"

void di_gen();
void test_processor_fpu();
void test_processor_fpu_integration();
void test_processor_fpu_sse();

int main()
{
    Diana_Init();
    DianaProcessor_GlobalInit();

    test_processor_fpu_sse();
    test_processor_a();
    test_processor_s();
    test_processor_r();
    test_processor_m();
    test_processor_l();
    test_processor_j();
    test_processor_i();
    test_processor_d();
    test_processor_c();
    test_processor_x();
    test_processor_o();
    test_processor_n();
    test_processor_call();
    test_processor();
    test_processor_flags();
    test_processor_b();
    test_processor_m_xmm();
    test_processor_fpu();
    test_processor_fpu_integration();
    return 0;
}
