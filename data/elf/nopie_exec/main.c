/* Test executable for a non-PIE (ET_EXEC) image (see build.sh and tests/cli/test_process.py):
   its symbol values and e_entry are absolute addresses, not offsets from where it is loaded.
   Prints "ready" and runs until stdin is closed, so a test can open it with --pid. */
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>

__attribute__((noinline)) int nopie_func_a(int x) { return x * 3 + 1; }
__attribute__((noinline)) int nopie_func_b(int x) { return nopie_func_a(x) ^ 0x55; }

int main(void)
{
    char c;
    /* lets a process that is not our parent (orthia) read our memory under Yama ptrace_scope=1 */
    prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
    printf("ready %d\n", nopie_func_b(1));
    fflush(stdout);
    while (read(0, &c, 1) > 0)
    {
    }
    return 0;
}
