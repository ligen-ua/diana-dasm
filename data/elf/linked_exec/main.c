/* Test executable for dependency linking in file mode (see build.sh and tests/cli/test_dependencies.py):
   imports exp_func_07 and exp_func_21 from libexports_gnu.so through PLT slots, and nothing else,
   so the only dependency is the library beside it. Not meant to run. */
extern int exp_func_07(void);
extern int exp_func_21(void);

int linked_main(void)
{
    return exp_func_07() + exp_func_21();
}

void _start(void)
{
    linked_main();
    for (;;)
    {
    }
}
