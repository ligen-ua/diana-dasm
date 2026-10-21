/* Test library for ELF startup routines (see build.sh and ot_elf_test.cpp):
   code that runs before main and that the analyzer must find without symbols.
   - exp_ifunc:   exported IFUNC, its resolver is the STT_GNU_IFUNC symbol value
   - local_ifunc: static IFUNC called internally, known only from R_*_IRELATIVE
   - startup_ctor: a .init_array entry
   - startup_init: DT_INIT (-Wl,-init) */

static int global_impl(void) { return 1; }
static void* global_resolver(void) { return (void*)global_impl; }
int exp_ifunc(void) __attribute__((ifunc("global_resolver")));

static int local_impl(void) { return 2; }
static void* local_resolver(void) { return (void*)local_impl; }
static int local_ifunc(void) __attribute__((ifunc("local_resolver")));
int call_local_ifunc(void) { return local_ifunc(); }

static int g_counter;
__attribute__((constructor)) static void startup_ctor(void) { ++g_counter; }
void startup_init(void) { g_counter += 2; }
