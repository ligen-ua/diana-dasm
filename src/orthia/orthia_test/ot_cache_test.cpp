#include "test_common.h"
#include "orthia_memory_cache.h"


static void test_memory_cache1(int pageSize)
{
    std::vector<char> initialPage(0x1000);
    initialPage[0] = 0x55;
    initialPage[1] = (char)0x88;
    initialPage[initialPage.size()- 0x100-1] = 0x01;
    initialPage[initialPage.size()- 0x100] = 0x02;
    initialPage[initialPage.size()- 1] = 0x77;
    orthia::CReaderOverVector reader(0x2000, initialPage);
    orthia::CMemoryStorageOfModifiedData cache(&reader, pageSize);

    // read entire page
    std::vector<char> buffer(0x1000);
    orthia::Address_type bytesRead = 0;
    cache.Read(0x2000, 0x1001, &buffer.front(), &bytesRead, 0, 0, reg_none);
    DIANA_TEST_ASSERT(initialPage == buffer && bytesRead == buffer.size());

    // custom read 
    cache.Read(0x2001, 0x1, &buffer.front(), &bytesRead, 0, 0, reg_none);
    DIANA_TEST_ASSERT(buffer[0] == (char)0x88 && bytesRead == 1);

    // do write
    orthia::Address_type bytesWritten = 0;
    cache.Write(0x2001, 0x2, "\x11\x22", &bytesWritten, 0, 0, reg_none);
    DIANA_TEST_ASSERT(bytesWritten == 2);

    // full read
    std::vector<char> eqPage2(initialPage);
    eqPage2[1] = 0x11;
    eqPage2[2] = 0x22;
    cache.Read(0x2000, 0x1001, &buffer.front(), &bytesRead, 0, 0, reg_none);
    DIANA_TEST_ASSERT(eqPage2 == buffer && bytesRead == buffer.size());

    // custom read
    cache.Read(0x2001, 0x1, &buffer.front(), &bytesRead, 0, 0, reg_none);
    DIANA_TEST_ASSERT(buffer[0] == (char)0x11 && bytesRead == 1);

    // write to the end
    cache.Write(0x2FFF, 0x1, "\x33", &bytesWritten, 0, 0, reg_none);
    DIANA_TEST_ASSERT(bytesWritten == 1);

    // read custom
    std::vector<char> eqPage3(initialPage.begin() + 0xEFF, initialPage.end());
    eqPage3[eqPage3.size()-1] = '\x33';
    cache.Read(0x2EFF, 0x101, &buffer.front(), &bytesRead, 0, 0, reg_none);
    DIANA_TEST_ASSERT(bytesRead == 0x101);
    buffer.resize(eqPage3.size());
    DIANA_TEST_ASSERT(buffer == eqPage3);

    cache.Write(0x2FFE, 0x1, "\x44", &bytesWritten, 0, 0, reg_none);
    DIANA_TEST_ASSERT(bytesWritten == 1);

    // full read
    buffer.resize(initialPage.size()-2);
    cache.Read(0x2001, buffer.size(), &buffer.front(), &bytesRead, 0, 0, reg_none);
    DIANA_TEST_ASSERT(bytesRead == buffer.size());

    std::vector<char> eqPage4(initialPage.begin() + 1, initialPage.end()-1);
    eqPage4[0] = 0x11;
    eqPage4[1] = 0x22;
    eqPage4[eqPage4.size()-1] = '\x44';

    DIANA_TEST_ASSERT(buffer == eqPage4);

    // write to the middle, more than page
    std::vector<char> massiveWrite(0x207);
    for(int i = 0; i < 0x207; ++i)
    {
        massiveWrite[i] = (char)i | (char)0x80;
    }
    cache.Write(0x23F0, massiveWrite.size(), &massiveWrite.front(), &bytesWritten, 0, 0, reg_none);

    // test it
    buffer.resize(initialPage.size()-2);
    cache.Read(0x2001, buffer.size(), &buffer.front(), &bytesRead, 0, 0, reg_none);
    DIANA_TEST_ASSERT(bytesRead == buffer.size());

    std::vector<char> eqPage5(initialPage.begin() + 1, initialPage.end()-1);
    eqPage5[0] = 0x11;
    eqPage5[1] = 0x22;
    eqPage5[eqPage5.size()-1] = '\x44';
    memcpy(&eqPage5.front()+0x3F0-1, &massiveWrite.front(), massiveWrite.size());

    DIANA_TEST_ASSERT(buffer == eqPage5);
}

static void test_memory_cache2()
{
    orthia::CReaderOverVector reader(0, 0);
    orthia::CMemoryStorageOfModifiedData cache(&reader);
    
    std::vector<char> write(0x207);
    orthia::Address_type bytesWritten = 0, bytesRead = 0;
    cache.Write(0x23F0, 
                write.size(), 
                &write.front(), 
                &bytesWritten, 
                ORTHIA_MR_FLAG_WRITE_ALLOCATE, 
                0, 
                reg_none);

    std::vector<char> data(0x10000);
    cache.Read(0x100, 0x10000, &data.front(), &bytesRead, 0, 0, reg_none);
    DIANA_TEST_ASSERT(bytesRead == 0);
}
void test_memory_cache()
{
    DIANA_TEST(test_memory_cache1(0x100));
    DIANA_TEST(test_memory_cache2());
}