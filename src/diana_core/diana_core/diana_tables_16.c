#include "diana_tables_16.h"
#include "diana_tables_common.h"

// 16 bit
//[BX + SI]            000   00    08    10    18    20    28    30    38
//[BX + DI]            001   01    09    11    19    21    29    31    39
//[BP + SI]            010   02    0A    12    1A    22    2A    32    3A
//[BP + DI]            011   03    0B    13    1B    23    2B    33    3B
//[SI]             00  100   04    0C    14    1C    24    2C    34    3C
//[DI]                 101   05    0D    15    1D    25    2D    35    3D
//disp16               110   06    0E    16    1E    26    2E    36    3E
//[BX]                 111   07    0F    17    1F    27    2F    37    3F
//
//[BX+SI]+disp8        000   40    48    50    58    60    68    70    78
//[BX+DI]+disp8        001   41    49    51    59    61    69    71    79
//[BP+SI]+disp8        010   42    4A    52    5A    62    6A    72    7A
//[BP+DI]+disp8        011   43    4B    53    5B    63    6B    73    7B
//[SI]+disp8       01  100   44    4C    54    5C    64    6C    74    7C
//[DI]+disp8           101   45    4D    55    5D    65    6D    75    7D
//[BP]+disp8           110   46    4E    56    5E    66    6E    76    7E
//[BX]+disp8           111   47    4F    57    5F    67    6F    77    7F
//
//[BX+SI]+disp16       000   80    88    90    98    A0    A8    B0    B8
//[BX+DI]+disp16       001   81    89    91    99    A1    A9    B1    B9
//[BX+SI]+disp16       010   82    8A    92    9A    A2    AA    B2    BA
//[BX+DI]+disp16       011   83    8B    93    9B    A3    AB    B3    BB
//[SI]+disp16      10  100   84    8C    94    9C    A4    AC    B4    BC
//[DI]+disp16          101   85    8D    95    9D    A5    AD    B5    BD
//[BP]+disp16          110   86    8E    96    9E    A6    AE    B6    BE
//[BX]+disp16          111   87    8F    97    9F    A7    AF    B7    BF
//
//EAX/AX/AL            000   C0    C8    D0    D8    E0    E8    F0    F8
//ECX/CX/CL            001   C1    C9    D1    D9    E1    E9    F1    F9
//EDX/DX/DL            010   C2    CA    D2    DA    E2    EA    F2    FA
//EBX/BX/BL            011   C3    CB    D3    DB    E3    EB    F3    FB
//ESP/SP/AH        11  100   C4    CC    D4    DC    E4    EC    F4    FC
//EBP/BP/CH            101   C5    CD    D5    DD    E5    ED    F5    FD
//ESI/SI/DH            110   C6    CE    D6    DE    E6    EE    F6    FE
//EDI/DI/BH            111   C7    CF    D7    DF    E7    EF    F7    FF

typedef struct _dianaRM16
{
    DianaUnifiedRegister reg1;
    DianaUnifiedRegister reg2;
    DI_CHAR dispSize;
}DianaRM16;

static DianaRM16 g_table1[3][8]= 
{
    {
        {reg_BX, reg_SI, 0},
        {reg_BX, reg_DI, 0},
        {reg_BP, reg_SI, 0},
        {reg_BP, reg_DI, 0},
        {reg_SI, reg_none, 0},
        {reg_DI, reg_none, 0},
        {reg_none, reg_none, 2},
        {reg_BX, 0, 0}
    },
    {
        {reg_BX, reg_SI, 1},
        {reg_BX, reg_DI, 1},
        {reg_BP, reg_SI, 1},
        {reg_BP, reg_DI, 1},
        {reg_SI, reg_none, 1},
        {reg_DI, reg_none, 1},
        {reg_BP, reg_none, 1},
        {reg_BX, reg_none, 1}
    },
    {
        {reg_BX, reg_SI, 2},
        {reg_BX, reg_DI, 2},
        {reg_BX, reg_SI, 2},
        {reg_BX, reg_DI, 2},
        {reg_SI, reg_none, 2},
        {reg_DI, reg_none, 2},
        {reg_BP, reg_none, 2},
        {reg_BX, reg_none, 2}
    }
};

int Diana_ReadIndexStructure16(DianaContext * pContext,
                               DianaLinkedOperand * pInfo,
                               DI_CHAR iOpSize,
                               unsigned char postByte,
                               DianaReadStream * pStream,
                               DianaOperandValue * pValue,
                               DianaValueType * pType)
{

    DI_CHAR mod = Diana_GetMod(postByte);
    DI_CHAR rm = Diana_GetRm(postByte);

    pStream;

    if (mod == 3)
    {
        *pType = diana_register;
        return Diana_DispatchMod3(pInfo, 
                                  rm, 
                                  rm, 
                                  iOpSize, 
                                  &pValue->recognizedRegister,
                                  pContext->iRexPrefix);
    }

    if (mod>3)
        return DI_ERROR;

    *pType = diana_index;

    {
        DianaRmIndex  * pIndex = &pValue->rmIndex;
        DianaRM16 * pOp = &g_table1[mod][rm];
        
        pIndex->dispSize = pOp->dispSize;
        pIndex->dispValue = 0;
        pIndex->index = 1;
        pIndex->indexed_reg =  pOp->reg1;
        if (pIndex->indexed_reg == reg_none)
            pIndex->index = 0;

        pIndex->reg =  pOp->reg2;
    }
    return DI_SUCCESS;
}
