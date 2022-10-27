#include "diana_win32_executable_heap.h"
#include "diana_win32_sys_memcpy.h"

#ifdef DIANA_HAS_WIN32

#pragma warning(disable:4055)
#pragma warning(disable:4054)
static const DI_CHAR memcpy_core_32[] = 
{
    0x55,                   // push        ebp  
    0x8B, 0xEC,             // mov         ebp,esp 
    0x1E,                   // push        ds   
    0x06,                   // push        es   
    0x57,                   // push        edi  
    0x56,                   // push        esi  
    0x51,                   // push        ecx  
    0x66, 0x8C, 0xDE,       // mov         si,ds 
    0x66, 0x8E, 0xC6,       // mov         es,si 
    0x66, 0x8E, 0x5D, 0x14, // mov         ds,word ptr [selector16] 
    0x8B, 0x7D, 0x08,       // mov         edi,dword ptr [pBuffer] 
    0x8B, 0x75, 0x0C,       // mov         esi,dword ptr [offset32] 
    0x8B, 0x4D, 0x10,       // mov         ecx,dword ptr [iBufferSize32] 
    0xFC,                   // cld
    0xF3, 0xA4,             // rep movs    byte ptr es:[edi],byte ptr [esi] 
    0x59,                   // pop         ecx  
    0x5E,                   // pop         esi  
    0x5F,                   // pop         edi  
    0x07,                   // pop         es
    0x1F,                   // pop         ds
    0x8B, 0xE5,             // mov         esp,ebp 
    0x5D,                   // pop         ebp  
    0xC3                    // ret              
};


static const DI_CHAR memcpy_core_64[] = 
{
0x57,                                               // push    rdi
0x56,                                               // push    rsi
0x66, 0x8C, 0xE8,                                   // mov     ax, gs
0x66, 0x50,                                         // push    ax
0x48, 0x8B, 0xF9,                                   // mov     rdi, rcx
0x48, 0x8B, 0xF2,                                   // mov     rsi, rdx
0x66, 0x41, 0x8E, 0xE9,                             // mov     gs, r9w
0x49, 0x8B, 0xC8,                                   // mov     rcx, r8
0x48, 0x85, 0xC9,                                   // test    rcx, rcx
0x74, 0x22,                                         // jz      short loc_3D
0x48, 0x83, 0xF9, 0x08,                             // cmp    rcx, 8
0x74, 0x12,                                         // jz      short loc_34
0x65, 0x8A, 0x06,                                   // mov     al, gs:[rsi]
0x88, 0x07,                                         // mov     [rdi], al
0x48, 0xFF, 0xC6,                                   // inc     rsi
0x48, 0xFF, 0xC7,                                   // inc     rdi
0x48, 0xFF, 0xC9,                                   // dec     rcx
0x74, 0x09,                                         // jz      short loc_3D
0xEB, 0xEE,                                         // jmp     short loc_22
0x65, 0x48, 0x8B, 0x06,                             // mov     rax, gs:[esi]
0x48, 0x89, 0x07,                                   // mov     [edi], rax
0x66, 0x58,                                         // pop     ax
0x66, 0x8E, 0xE8,                                   // mov     gs, ax
0x5E,                                               // pop     rsi
0x5F,                                               // pop     rdi
0xC3                                                // retn
};


#ifdef DIANA_CFG_I386
sys_memcpy_ptr_type  DianaWin32_AllocSysMemcpy()
{
    void * pResult = DianaWin32ExecutableHeap_Alloc(sizeof(memcpy_core_32));
    if (!pResult)
    {
        return 0;
    }
    DIANA_MEMCPY(pResult, memcpy_core_32, sizeof(memcpy_core_32));
    return (sys_memcpy_ptr_type)pResult;
}
#else
sys_memcpy_ptr_type  DianaWin32_AllocSysMemcpy()
{
    void * pResult = DianaWin32ExecutableHeap_Alloc(sizeof(memcpy_core_64));
    if (!pResult)
    {
        return 0;
    }
    DIANA_MEMCPY(pResult, memcpy_core_64, sizeof(memcpy_core_64));
    return (sys_memcpy_ptr_type)pResult;
}
#endif
void  DianaWin32_FreeSysMemcpy(sys_memcpy_ptr_type p)
{
    if (!p)
    {
        return;
    }
    DianaWin32ExecutableHeap_Free((void*)p);
}

#endif