#include "diana_processor_cmd_l_xmm.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_processor_cmd_internal_xmm.h"

int Diana_Call_lddqu(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm(pDianaContext,
                           pCallContext));
    DI_PROC_END
}
