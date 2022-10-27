extern "C"
{
#include "diana_win32_streams.h"
}

#pragma warning(disable:4733)
#include "test_common.h"


void test_streams_ds_read()
{
    int test = 42;
    int test2 = 0;

    OPERAND_SIZE readBytes = 0;

    DianaWin32Stream stream;
    DianaWin32Stream_Init(&stream);

    unsigned short dsValue = 0;
    {
    __asm mov dsValue, ds
    }

    DIANA_TEST_ASSERT(stream.m_parent.pReadFnc(&stream.m_parent, 
                             dsValue,
                             (OPERAND_SIZE)&test,
                             &test2,
                             sizeof(test2),
                             &readBytes,
                             0,
                             reg_DS)==DI_SUCCESS);

    DIANA_TEST_ASSERT(test == test2);



}

void test_streams_fs_read()
{
    unsigned int test1 = 0;
    unsigned int test2 = 0;

    OPERAND_SIZE readBytes = 0;

    DianaWin32Stream stream;
    DianaWin32Stream_Init(&stream);

    unsigned short fsValue = 0;
    {
    __asm mov fsValue, fs
    __asm mov eax, dword ptr fs:[0]
    __asm mov test1, eax
    }

    DIANA_TEST_ASSERT(stream.m_parent.pReadFnc(&stream.m_parent, 
                             fsValue,
                             0,
                             &test2,
                             sizeof(test2),
                             &readBytes,
                             0,
                             reg_FS)==DI_SUCCESS);

    DIANA_TEST_ASSERT(test1 == test2);
}


void test_streams_ds_write()
{
    int test = 42;
    int test2 = 0;

    OPERAND_SIZE readBytes = 0;

    DianaWin32Stream stream;
    DianaWin32Stream_Init(&stream);

    unsigned short dsValue = 0;
    {
    __asm mov dsValue, ds
    }

    DIANA_TEST_ASSERT(stream.m_parent.pWriteFnc(&stream.m_parent, 
                             dsValue,
                             (OPERAND_SIZE)&test2,
                             &test,
                             sizeof(test),
                             &readBytes,
                             0,
                             reg_DS)==DI_SUCCESS
                             );

    DIANA_TEST_ASSERT(test == test2);



}

void test_streams_fs_write()
{

    OPERAND_SIZE readBytes = 0;

    DianaWin32Stream stream;
    DianaWin32Stream_Init(&stream);

    unsigned short fsValue = 0;
    unsigned int oldFs0Value = 0;
    unsigned int newFs0Value = 14413;
    unsigned int changedFs0Value = 0;

    __asm mov fsValue, fs
    __asm mov eax, dword ptr fs:[0]
    __asm mov oldFs0Value, eax

    DIANA_TEST_ASSERT(stream.m_parent.pWriteFnc(&stream.m_parent, 
                             fsValue,
                             0,
                             &newFs0Value,
                             sizeof(newFs0Value),
                             &readBytes,
                             0,
                             reg_FS)==DI_SUCCESS);


    __asm mov eax, dword ptr fs:[0]
    __asm mov changedFs0Value, eax
 
    DIANA_TEST_ASSERT(changedFs0Value == newFs0Value);

    __asm mov eax, oldFs0Value
    __asm mov dword ptr fs:[0], eax
}

void test_streams()
{
    test_streams_ds_read();
    test_streams_fs_read();
    test_streams_ds_write();
    test_streams_fs_write();
}