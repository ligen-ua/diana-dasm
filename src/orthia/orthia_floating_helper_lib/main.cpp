// A shared library for the floating code tests (tests/cli/test_floating.py): orthia_floating_helper
// loads it and writes a jump over the start of floating_lib_function into the handler allocation.
// So a second module leads into a floating region the helper's own hook leads to as well.

#if defined(_WIN32)
#define LIB_EXPORT extern "C" __declspec(dllexport)
#define LIB_NOINLINE __declspec(noinline)
#else
#define LIB_EXPORT extern "C" __attribute__((visibility("default")))
#define LIB_NOINLINE __attribute__((noinline))
#endif

static volatile int g_sink = 0;

// longer than the jump the helper writes over its start (16 bytes at most)
LIB_EXPORT LIB_NOINLINE int floating_lib_function(int value)
{
    int result = value;
    for (int i = 0; i < 8; ++i)
    {
        g_sink = g_sink + result;
        result = result * 3 + g_sink;
    }
    return result;
}
