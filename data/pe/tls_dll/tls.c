/* Test DLL for TLS callback analysis (see build.sh and ot_pe_test.cpp).
   tls_callback is reached only through the TLS directory: no code calls it.
   No CRT: the TLS directory (_tls_used) and the callback array are built here. */

typedef void (__stdcall *TlsCallback)(void* module, unsigned long reason, void* reserved);

static volatile int g_reasons;
static void __stdcall tls_callback(void* module, unsigned long reason, void* reserved)
{
    if (reason == 1)
        g_reasons += 2;
    else
        g_reasons -= 1;
}

#pragma section(".CRT$XLA", read)
#pragma section(".CRT$XLB", read)
#pragma section(".CRT$XLZ", read)
__declspec(allocate(".CRT$XLA")) const TlsCallback __xl_a = 0;
__declspec(allocate(".CRT$XLB")) const TlsCallback __xl_b = tls_callback;
__declspec(allocate(".CRT$XLZ")) const TlsCallback __xl_z = 0;

#pragma section(".tls", read, write)
#pragma section(".tls$ZZZ", read, write)
__declspec(allocate(".tls")) char _tls_start = 0;
__declspec(allocate(".tls$ZZZ")) char _tls_end = 0;
unsigned long _tls_index = 0;

/* IMAGE_TLS_DIRECTORY64 or IMAGE_TLS_DIRECTORY32: pointer-sized addresses */
#ifdef _WIN64
typedef unsigned long long TlsPointer;
#else
typedef unsigned int TlsPointer;
#endif
typedef struct
{
    TlsPointer startOfRawData, endOfRawData, addressOfIndex, addressOfCallBacks;
    unsigned int sizeOfZeroFill, characteristics;
} TlsDirectory;
const TlsDirectory _tls_used = {
    (TlsPointer)&_tls_start, (TlsPointer)&_tls_end,
    (TlsPointer)&_tls_index, (TlsPointer)(&__xl_a + 1), 0, 0 };

int __stdcall DllMain(void* module, unsigned long reason, void* reserved) { return 1; }
__declspec(dllexport) int tls_reasons(void) { return g_reasons; }
