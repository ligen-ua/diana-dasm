#ifndef TEST_PATCHERS_H
#define TEST_PATCHERS_H

void test_patchers();
#ifndef DIANA_NO_ARM
// AArch64 hooks, test_patchers_arm.cpp
void test_patchers_arm();
#endif

#endif
