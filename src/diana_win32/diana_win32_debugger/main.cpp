#include "diana_debugger.h"

#include "iostream"

extern "C"
{
#include "diana_win32_core_init.h"
#include "diana_processor/diana_processor_commands.h"
#include "diana_processor/diana_processor_core_impl.h"
#include "diana_core_gen_tags.h"
}


static 
int Diana_Call_sysenter(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}


static 
int Diana_Call_int(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_wait(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}


static 
int Diana_Call_fnstcw(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_fnclex(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}
static 
int Diana_Call_cpuid(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_movapd(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_movdqa(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_pxor(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_fldcw(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}
static 
int Diana_Call_fild(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_fmul(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_fstp(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}


static 
int Diana_Call_fst(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}


static 
int Diana_Call_fld(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}


static 
int Diana_Call_fcomp(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}


static 
int Diana_Call_fnstsw(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_faddp(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_fsqrt(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_fcom(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_fsubr(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}

static 
int Diana_Call_fcompp(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    return DI_ERROR_NOT_USED_BY_CORE;
}







static
void DianaWin32Processor_OnGroup(DianaGroupInfo * p)
{
    switch (p->m_commandId)
    {
    DI_PROC_REGISTER_COMMAND(sysenter)
    DI_PROC_REGISTER_COMMAND(int)
    DI_PROC_REGISTER_COMMAND(wait)
    DI_PROC_REGISTER_COMMAND(fnstcw)
    DI_PROC_REGISTER_COMMAND(fnclex)
    DI_PROC_REGISTER_COMMAND(cpuid)
    DI_PROC_REGISTER_COMMAND(movapd)
    DI_PROC_REGISTER_COMMAND(movdqa)
    DI_PROC_REGISTER_COMMAND(pxor)
    DI_PROC_REGISTER_COMMAND(fldcw)
    DI_PROC_REGISTER_COMMAND(fild)
    DI_PROC_REGISTER_COMMAND(fmul)
    DI_PROC_REGISTER_COMMAND(fstp)
    DI_PROC_REGISTER_COMMAND(fst)
    DI_PROC_REGISTER_COMMAND(fld)
    DI_PROC_REGISTER_COMMAND(fcomp)
    DI_PROC_REGISTER_COMMAND(fnstsw)

    DI_PROC_REGISTER_COMMAND(faddp)
    DI_PROC_REGISTER_COMMAND(fsqrt)
    DI_PROC_REGISTER_COMMAND(fcom)
    DI_PROC_REGISTER_COMMAND(fsubr)
    DI_PROC_REGISTER_COMMAND(fcompp)
    }
}

static void DianaWin32Processor_LinkCommands()
{
    DianaGroupInfo * p = Diana_GetGroups();
    long i = 0, count = Diana_GetGroupsCount();

    for(; i < count; ++i)
    {
        DianaWin32Processor_OnGroup(p);
        ++p;
    }
}
int wmain(int argc, wchar_t * argv[])
{
    if (argc != 2)
    {
        std::cout<<"Usage: exe_file\n";
        return ERROR_INVALID_FUNCTION;
    }

    Diana_Init();
    DianaProcessor_GlobalInit();

    DianaWin32Processor_LinkCommands();


    try
    {
        diana::CDianaDebugger dbg(argv[1]);
        dbg.Start();
    }
    catch(diana::Win32Exception & e)
    {
        std::cout<<e.what()<<". Error: "<<e.m_error<<"\n";
    }
    catch(std::exception & e)
    {
        std::cout<<e.what()<<"\n";
    }
    return NO_ERROR;
}