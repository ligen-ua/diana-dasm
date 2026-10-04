/* Test library for ELF export enumeration (see build.sh and ot_elf_test.cpp).
   Exports: exp_func_00..exp_func_39, exp_data_object, exp_weak_func, exp_ifunc,
   vfunc_v1, vfunc_v2 and vfunc (vfunc@VER_1 hidden, vfunc@@VER_2 default).
   Not exported: static/hidden functions and the undefined ext_import_* symbols. */

/* undefined: become unhashed .dynsym entries, so GNU hash symoffset > 1 */
extern int ext_import_1(void);
extern int ext_import_2(void);
extern int ext_import_3(void);

#define EXP_FUNC(n) int exp_func_##n(void) { return 0x##n; }
EXP_FUNC(00) EXP_FUNC(01) EXP_FUNC(02) EXP_FUNC(03) EXP_FUNC(04)
EXP_FUNC(05) EXP_FUNC(06) EXP_FUNC(07) EXP_FUNC(08) EXP_FUNC(09)
EXP_FUNC(10) EXP_FUNC(11) EXP_FUNC(12) EXP_FUNC(13) EXP_FUNC(14)
EXP_FUNC(15) EXP_FUNC(16) EXP_FUNC(17) EXP_FUNC(18) EXP_FUNC(19)
EXP_FUNC(20) EXP_FUNC(21) EXP_FUNC(22) EXP_FUNC(23) EXP_FUNC(24)
EXP_FUNC(25) EXP_FUNC(26) EXP_FUNC(27) EXP_FUNC(28) EXP_FUNC(29)
EXP_FUNC(30) EXP_FUNC(31) EXP_FUNC(32) EXP_FUNC(33) EXP_FUNC(34)
EXP_FUNC(35) EXP_FUNC(36) EXP_FUNC(37) EXP_FUNC(38) EXP_FUNC(39)

int exp_data_object = 42;

__attribute__((weak)) int exp_weak_func(void) { return 1; }

static int ifunc_impl(void) { return 2; }
static void* ifunc_resolver(void) { return (void*)ifunc_impl; }
int exp_ifunc(void) __attribute__((ifunc("ifunc_resolver")));

static int static_helper(void) { return ext_import_1() + ext_import_2() + ext_import_3(); }
__attribute__((visibility("hidden"))) int hidden_func(void) { return static_helper(); }

int vfunc_v1(void) { return hidden_func(); }
int vfunc_v2(void) { return 2; }
__asm__(".symver vfunc_v1,vfunc@VER_1");
__asm__(".symver vfunc_v2,vfunc@@VER_2");
