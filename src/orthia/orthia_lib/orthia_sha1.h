#ifndef ORTHIA_SHA1_H
#define ORTHIA_SHA1_H

#ifdef __cplusplus
extern "C"{
#endif

/*
 * If you do not have the ISO standard stdint.h header file, then you
 * must typdef the following:
 *    name              meaning
 *  uint32_t         unsigned 32 bit integer
 *  uint8_t          unsigned 8 bit integer (i.e., unsigned char)
 *  int_least16_t    integer of >= 16 bits
 *
 */

typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef short int_least16_t;

#ifndef _SHA_enum_
#define _SHA_enum_
enum
{
    shaSuccess = 0,
    shaNull,            /* Null pointer parameter */
    shaInputTooLong,    /* input data too long */
    shaStateError       /* called Input after Result */
};
#endif
#define SHA1HashSize 20

/*
 *  This structure will hold context information for the SHA-1
 *  hashing operation
 */
typedef struct SHA1Context
{
    uint32_t Intermediate_Hash[SHA1HashSize/4]; /* Message Digest  */

    uint32_t Length_Low;            /* Message length in bits      */
    uint32_t Length_High;           /* Message length in bits      */

                               /* Index into message block array   */
    int_least16_t Message_Block_Index;
    uint8_t Message_Block[64];      /* 512-bit message blocks      */

    int Computed;               /* Is the digest computed?         */
    int Corrupted;             /* Is the message digest corrupted? */
} SHA1Context;

/*
 *  Function Prototypes
 */


int __stdcall SHA1Reset(  SHA1Context *);
int __stdcall SHA1Input(  SHA1Context *,
                        const uint8_t *,
                        unsigned int);
int __stdcall SHA1Result( SHA1Context *,
                        uint8_t Message_Digest[SHA1HashSize]);

#define SHA1_HASH_SIZE 20
void __stdcall Sha1Hash(const unsigned char * pData, unsigned int iSize, uint8_t * pResult);

#ifdef __cplusplus
}; //extern "C"

template<class Type>
int __stdcall SHA1Input2(SHA1Context    *context,
                        Type * pObject)
{
    return SHA1Input(context, (uint8_t*)pObject, sizeof(Type));
}

template<class Type>
int __stdcall SHA1Input2(SHA1Context    *context,
                        Type * pObject,
                        size_t size)
{
    return SHA1Input(context, (uint8_t*)pObject, (unsigned int)(sizeof(Type)*size));
}

#endif

#endif
