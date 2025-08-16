#define _CRT_SECURE_NO_WARNINGS
#include "orthia_dbgext.h"
#include "algorithm"
extern "C"
{
#include "diana_core_win32_context.h"
#include "diana_processor/diana_processor_win32_context.h"
}
#undef min

WINDBG_EXTENSION_APIS   ExtensionApis = {};
static PDEBUG_CLIENT4        g_ExtClient = 0;
static PDEBUG_CONTROL        g_ExtControl = 0;
static PDEBUG_CONTROL4       g_ExtControl4 = 0;
static PDEBUG_SYMBOLS2       g_ExtSymbols = 0;
static IDebugAdvanced * g_DebugAdvanced = 0;
static ULONG   g_TargetMachine = 0;
static BOOL    g_Connected = 0;


void
ExtRelease(void)
{
    g_ExtClient = NULL;
    EXT_RELEASE(g_ExtControl4);
    EXT_RELEASE(g_DebugAdvanced);
    EXT_RELEASE(g_ExtControl);
    EXT_RELEASE(g_ExtSymbols);
}


IDebugClient4 * ExtQueryClient()
{
    return g_ExtClient;
}
IDebugControl* ExtQueryControl()
{
    return g_ExtControl;
}
IDebugControl4* ExtQueryControl4()
{
    return g_ExtControl4;
}
HRESULT
ExtQuery(PDEBUG_CLIENT4 Client)
{
    HRESULT Status;

    if ((Status = Client->QueryInterface(__uuidof(IDebugControl),
                                 (void **)&g_ExtControl)) != S_OK)
    {
        goto Fail;
    }
    if ((Status = Client->QueryInterface(__uuidof(IDebugSymbols2),
                                (void **)&g_ExtSymbols)) != S_OK)
    {
        goto Fail;
    }
    if ((Status = Client->QueryInterface(__uuidof(IDebugAdvanced),
                                         (void **)&g_DebugAdvanced)) != S_OK)
    {
        goto Fail;
    }
    if ((Status = Client->QueryInterface(__uuidof(IDebugControl4),
        (void**)&g_ExtControl4)) != S_OK)
    {
        g_ExtControl4 = 0;
    }
    g_ExtClient = Client;

    return S_OK;

 Fail:
    ExtRelease();
    return Status;
}


extern "C"
HRESULT
CALLBACK
DebugExtensionInitialize(PULONG Version, PULONG Flags)
{
    IDebugClient *DebugClient;
    PDEBUG_CONTROL DebugControl;
    HRESULT Hr;

    *Version = DEBUG_EXTENSION_VERSION(1, 0);
    *Flags = 0;
    Hr = S_OK;

    if ((Hr = DebugCreate(__uuidof(IDebugClient),
                          (void **)&DebugClient)) != S_OK)
    {
        return Hr;
    }

    if ((Hr = DebugClient->QueryInterface(__uuidof(IDebugControl),
                                  (void **)&DebugControl)) == S_OK)
    {

        //
        // Get the windbg-style extension APIS
        //
        ExtensionApis.nSize = sizeof (ExtensionApis);
        Hr = DebugControl->GetWindbgExtensionApis64(&ExtensionApis);

        DebugControl->Release();

    }
    DebugClient->Release();
    return Hr;
}

extern "C"
void
CALLBACK
DebugExtensionUninitialize(void)
{
    return;
}

extern "C"
void
CALLBACK
DebugExtensionNotify(ULONG Notify, ULONG64 Argument)
{
    UNREFERENCED_PARAMETER(Argument);

    //
    // The first time we actually connect to a target
    //

    if ((Notify == DEBUG_NOTIFY_SESSION_ACCESSIBLE) && (!g_Connected))
    {
        IDebugClient *DebugClient;
        HRESULT Hr;
        PDEBUG_CONTROL DebugControl;

        if ((Hr = DebugCreate(__uuidof(IDebugClient),
                              (void **)&DebugClient)) == S_OK)
        {
            //
            // Get the architecture type.
            //

            if ((Hr = DebugClient->QueryInterface(__uuidof(IDebugControl),
                                       (void **)&DebugControl)) == S_OK)
            {
                if ((Hr = DebugControl->GetActualProcessorType(
                                             &g_TargetMachine)) == S_OK)
                {
                    g_Connected = TRUE;
                }
                //NotifyOnTargetAccessible(DebugControl);
                DebugControl->Release();
            }

            DebugClient->Release();
        }
    }


    if (Notify == DEBUG_NOTIFY_SESSION_INACTIVE)
    {
        g_Connected = FALSE;
        g_TargetMachine = 0;
    }

    return;
}

//IMAGE_FILE_MACHINE_I386
//IMAGE_FILE_MACHINE_AMD64
ULONG DbgExt_GetTargetMachine()
{
    if (!g_Connected) 
    {
        return 0;
    }
    return g_TargetMachine;
}

ULONG DbgExt_GetCurrentModeOfTargetMachine(bool * pWow64)
{
    if (!g_Connected ||  !g_ExtControl)
    {
        return 0;
    }
    *pWow64 = false;
    if (g_TargetMachine == IMAGE_FILE_MACHINE_AMD64)
    {
        if (S_OK == g_ExtControl->IsPointer64Bit())
        {
            return IMAGE_FILE_MACHINE_AMD64;
        }
        *pWow64 = true;
    }
    return g_TargetMachine;
}

bool DbgExt_GetNameByOffset(ULONG64 offset,
                            PSTR nameBuffer,
                            ULONG nameBufferSize,
                            PULONG pResultNameSize,
                            ULONG64 * pDisplacement)
{
    *pDisplacement = 0;
    *pResultNameSize = 0;
    if (!SUCCEEDED(g_ExtSymbols->GetNameByOffset(offset, 
                                                    nameBuffer, 
                                                    nameBufferSize, 
                                                    pResultNameSize, 
                                                    pDisplacement)))
    {
        char defaultValue[] = "<unknown>";
        strncat(nameBuffer, defaultValue, std::min<ULONG>(sizeof(defaultValue)/sizeof(defaultValue[0])-1, nameBufferSize));
        return false;
    }
    return true;
}

void DbgExt_ReadThrough(ULONG64 offset, 
                        ULONG64 bytesToRead,
                        void * pBuffer)
{
    ULONG bytes = 0; 
    IDebugDataSpaces4 * pSpaces = 0;
    HRESULT status = g_ExtClient->QueryInterface(__uuidof(IDebugDataSpaces4), (PVOID*)&pSpaces);
    if (!SUCCEEDED(status))
    {
        ReadMemory(offset, 
                       pBuffer, 
                       (ULONG)bytesToRead,
                        &bytes);
        return;
    }

    ULONG64 base = offset;
    ULONG64 lastBytesToRead = bytesToRead;
    while(lastBytesToRead)
    {
        ULONG size =0;
        if (!SUCCEEDED(pSpaces->GetValidRegionVirtual(base,
                                        (ULONG)lastBytesToRead,
                                        &base,
                                        &size)))
        {
            break;
        }
        ULONG64 validSize = size;
        if (base < offset)
        {
            validSize = size - (offset-base);
            base = offset;
        }
        if (base - offset >= bytesToRead)
        {
            break;
        }
        if (validSize > offset - base + bytesToRead)
        {
            validSize = offset - base + bytesToRead;
        }
        bytes = 0;
        if (!validSize)
        {
            break;
        }
        if (!ReadMemory(base, 
                       (char*)pBuffer+base-offset, 
                       (ULONG)validSize,
                        &bytes))
        {
            break;
        }
        if (bytes != validSize)
            break;
        base += validSize;
        lastBytesToRead = offset - base + bytesToRead;
    }

    pSpaces->Release();
}


ULONG64 DbgExt_GetRegionSize(ULONG64 offset)
{
    ULONG bytes = 0; 
    IDebugDataSpaces4 * pSpaces = 0;
    HRESULT status = g_ExtClient->QueryInterface(__uuidof(IDebugDataSpaces4), (PVOID*)&pSpaces);
    if (!SUCCEEDED(status))
    {
        return 0;
    }

    ULONG64 base = offset;
    ULONG size =0;
    pSpaces->GetValidRegionVirtual(offset,
                                    0x1000000,
                                    &base,
                                    &size);
    pSpaces->Release();
    if (base != offset)
    {
        return 0;
    }
    return size;
}
bool DbgExt_IsRegionFree(ULONG64 offset, 
                         ULONG64 size,
                         ULONG64 * startOfNewRegion)
{
    if (startOfNewRegion)
    {
        *startOfNewRegion = 0;
    }

    ULONG bytes = 0; 
    IDebugDataSpaces4 * pSpaces = 0;
    HRESULT status = g_ExtClient->QueryInterface(__uuidof(IDebugDataSpaces4), (PVOID*)&pSpaces);
    if (!SUCCEEDED(status))
    {
        return false;
    }

    ULONG64 validBase = offset;
    ULONG validSize =0;
    pSpaces->GetValidRegionVirtual(offset,
                                   (ULONG)size,
                                   &validBase,
                                   &validSize);
    pSpaces->Release();
    // [offset]              [offset+size]
    //           [validBase]
    if (validBase >= offset && 
        (validBase - offset) < size)
    {
        if (startOfNewRegion)
        {
            ULONG64 newEnd = validBase + (validSize - 1);
            *startOfNewRegion = newEnd;
        }
        return false;
    }
    return true;
}
bool DbgExt_GetContext(DbgExt_Context_type * pType, std::vector<char> * pRawContext)
{
    pRawContext->clear();
    if (!g_DebugAdvanced)
    {
        return false;
    }

    *pType = decNone;
    bool wow64 = false;
    ULONG machine = DbgExt_GetCurrentModeOfTargetMachine(&wow64);
    int mode = 0;
    if (wow64)
    {
        *pType = decWOW;
        pRawContext->resize(sizeof(DIANA_CONTEXT_NTLIKE_32));
    }
    else
    {
        switch (machine)
        {
        case IMAGE_FILE_MACHINE_I386:
            *pType = decWin32;
            pRawContext->resize(sizeof(DIANA_CONTEXT_NTLIKE_32));
            break;
        case IMAGE_FILE_MACHINE_AMD64:
            *pType = decX64;
            pRawContext->resize(sizeof(DIANA_CONTEXT_NTLIKE_64));
            break;
        default:
            return false;
        }
    }

    HRESULT status = g_DebugAdvanced->GetThreadContext(&pRawContext->front(), (ULONG)pRawContext->size());
    if (!SUCCEEDED(status))
    {
        return false;
    }
    return true;
}

DbgExt_Context_type DbgExt_GetDianaContext(Diana_Processor_Registers_Context * pContext)
{
    DbgExt_Context_type type = decNone;
    std::vector<char> context;
    if (!DbgExt_GetContext(&type, &context))
    {
        return decNone;
    }

    if (context.empty())
    {
        return decNone;
    }

    switch(type)
    {
    case decWin32:
    case decWOW:
        if (context.size() != sizeof(DIANA_CONTEXT_NTLIKE_32))
        {
            return decNone;
        }
        if (DianaProcessor_ConvertContextToIndependent_Win32((DIANA_CONTEXT_NTLIKE_32 *)&context.front(), pContext))
        {
            return decNone;
        }
        break;
    case decX64:
        if (context.size() != sizeof(DIANA_CONTEXT_NTLIKE_64))
        {
            return decNone;
        }
        if (DianaProcessor_ConvertContextToIndependent_X64((DIANA_CONTEXT_NTLIKE_64 *)&context.front(), pContext))
        {
            return decNone;
        }
        break;
    default:
        return decNone;
    }
    return type;
}

int DbgExt_QueryDianaMode()
{
    DbgExt_Context_type type = decNone;
    std::vector<char> context;
    if (!DbgExt_GetContext(&type, &context))
    {
        return 0;
    }
    if (type == decNone)
    {
        return 0;
    }
    return type == decX64 ? DIANA_MODE64 : DIANA_MODE32;
}