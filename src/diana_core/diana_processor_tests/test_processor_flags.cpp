#include "test_processor_flags.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"

static unsigned char test_add[]=
{
  0xBC, 0x00, 0x08, 0x00, 0x00  // mov         esp,800h     
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0x3F                    // mov         al,3Fh 
, 0xB3, 0x01                    // mov         bl,1 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0xFD                    // mov         al,0FDh 
, 0xB3, 0x05                    // mov         bl,5 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0xFC                    // mov         al,0FCh 
, 0xB3, 0x05                    // mov         bl,5 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0x40                    // mov         al,40h 
, 0xB3, 0xC0                    // mov         bl,0C0h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf       
, 0x50                          // push        eax    
, 0x33, 0xC0                    // xor         eax,eax   
, 0xB0, 0xE0                    // mov         al,0E0h 
, 0xB3, 0x60                    // mov         bl,60h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0x60                    // mov         al,60h 
, 0xB3, 0xE0                    // mov         bl,0E0h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0xE0                    // mov         al,0E0h 
, 0xB3, 0xE0                    // mov         bl,0E0h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0xC0                    // mov         al,0C0h 
, 0xB3, 0xC0                    // mov         bl,0C0h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0xFF                    // mov         al,0FFh 
, 0xB3, 0x01                    // mov         bl,1 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0xFF                    // mov         al,0FFh 
, 0xB3, 0x81                    // mov         bl,81h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0xFF                    // mov         al,0FFh 
, 0xB3, 0xC1                    // mov         bl,0C1h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0xFF                    // mov         al,0FFh 
, 0xB3, 0x41                    // mov         bl,41h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0x40                    // mov         al,40h 
, 0xB3, 0x40                    // mov         bl,40h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0x60                    // mov         al,60h 
, 0xB3, 0x60                    // mov         bl,60h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0x7F                    // mov         al,7Fh 
, 0xB3, 0x01                    // mov         bl,1 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
, 0x33, 0xC0                    // xor         eax,eax
, 0xB0, 0x80                    // mov         al,80h 
, 0xB3, 0x80                    // mov         bl,80h 
, 0x02, 0xC3                    // add         al,bl 
, 0x9C                    // pushf            
, 0x50                          // push        eax  
};

const unsigned long g_realData[] ={
 0x00000000, 0x00000a47, 0x00000080, 0x00000a92,
 0x000000c0, 0x00000a86, 0x00000080, 0x00000a82, 
 0x00000040, 0x00000213, 0x000000c0, 0x00000297,  
 0x00000080, 0x00000293, 0x00000000, 0x00000257,  
 0x00000080, 0x00000283, 0x000000c0, 0x00000287,  
 0x00000040, 0x00000203, 0x00000040, 0x00000203,  
 0x00000000, 0x00000247, 0x00000001, 0x00000213,  
 0x00000002, 0x00000213, 0x00000040, 0x00000212
};

// todo write test with contextes
static void test_processor1()
{
    std::vector<unsigned char> buf(test_add, 
                                   test_add + sizeof(test_add));
    buf.resize(0x804);
    CTestProcessor proc(&buf.front(), (int)buf.size());

    DianaProcessor * pCallContext = proc.GetSelf();
    CLEAR_FLAG_ID;
        
    while(1)
    {
        int res = proc.ExecOnce();
        
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        if (GET_REG_RIP >= sizeof(test_add))
            break;
    }

    // analyze flags
    unsigned int * pFlags = (unsigned int * )(&buf[0x800]);
    unsigned int * pOur = pFlags-1;

    const unsigned long * pOriginal = g_realData+sizeof(g_realData)/sizeof(g_realData[0])-1;

    while(1)
    {
        DIANA_TEST_ASSERT(*pOur == *pOriginal);
        --pOriginal;
        --pOur;

        if (pOriginal < g_realData)
            break;
    }
}

static unsigned char test_sub[]=
{
  0xBC, 0x00, 0x08, 0x00, 0x00   // mov         esp,800h                         
, 0xB0, 0xE0                  // mov         al,0E0h 
, 0xB3, 0x20                  // mov         bl,20h 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x3F                  // mov         al,3Fh 
, 0xB3, 0xFF                  // mov         bl,0FFh 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x83                  // mov         al,83h 
, 0xB3, 0x9A                  // mov         bl,9Ah 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x80                  // mov         al,80h 
, 0xB3, 0x81                  // mov         bl,81h 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x80                  // mov         al,80h 
, 0xB3, 0xC0                  // mov         bl,0C0h 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x8A                  // mov         al,8Ah 
, 0xB3, 0xA5                  // mov         bl,0A5h 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x80                  // mov         al,80h 
, 0xB3, 0x40                  // mov         bl,40h 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x80                  // mov         al,80h 
, 0xB3, 0x01                  // mov         bl,1 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x40                  // mov         al,40h 
, 0xB3, 0xC0                  // mov         bl,0C0h 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x60                  // mov         al,60h 
, 0xB3, 0xA0                  // mov         bl,0A0h 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x7F                  // mov         al,7Fh 
, 0xB3, 0xFF                  // mov         bl,0FFh 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
, 0x33, 0xC0                  // xor         eax,eax 
, 0xB0, 0x73                  // mov         al,73h 
, 0xB3, 0xB7                  // mov         bl,0B7h 
, 0x2A, 0xC3                  // sub         al,bl 
, 0x9C                  // pushf            
, 0x50                     // push        eax  
};

const unsigned long g_realData2[] ={
0x000000bc, 0x00000a93, 0x00000080, 0x00000a83, 0x000000c0,
0x00000a87, 0x00000080, 0x00000a83, 0x0000007f, 0x00000a12,
0x00000040, 0x00000a02, 0x000000e5, 0x00000283, 0x000000c0,
0x00000287, 0x000000ff, 0x00000297, 0x000000e9, 0x00000293,
0x00000040, 0x00000203, 0x000000c0, 0x00000286, 0x00000000
};

static void test_processor2()
{
    std::vector<unsigned char> buf(test_sub, 
                                   test_sub + sizeof(test_sub));
    buf.resize(0x804);
    unsigned int * pFlags = (unsigned int * )(&buf[0x800]);

    CTestProcessor proc(&buf.front(), (int)buf.size());

    DianaProcessor * pCallContext = proc.GetSelf();
    CLEAR_FLAG_ID;
        
    while(1)
    {
        int res = proc.ExecOnce();
        
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        if (GET_REG_RIP >= sizeof(test_sub))
            break;
    }

    // analyze flags
    unsigned int * pOur = pFlags;

    const unsigned long * pOriginal = g_realData2+sizeof(g_realData2)/sizeof(g_realData2[0])-1;

    while(1)
    {
        DIANA_TEST_ASSERT(*pOur == *pOriginal);
        --pOriginal;
        --pOur;

        if (pOriginal < g_realData2)
            break;
    }
}

void test_processor_flags()
{
    DIANA_TEST(test_processor1());
    DIANA_TEST(test_processor2());
}