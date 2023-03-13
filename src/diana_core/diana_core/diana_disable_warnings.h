#ifdef _MSC_VER 

#pragma warning( disable: 4055 ) // 'conversion' : from data pointer 'type1' to function pointer 'type2'
#pragma warning( disable: 4146 ) // warning C4146: unary minus operator applied to unsigned type, result still unsigned
#pragma warning( disable: 4152 ) // non standard extension, function/data ptr conversion in expression

#ifdef DIANA_DISABLE_EXTENSION_WARNINGS

#pragma warning( disable: 4200 ) // warning C4200: nonstandard extension used: zero-sized array in struct/union
#pragma warning( disable: 4201 ) // warning C4201: nonstandard extension used: nameless struct/union
#pragma warning( disable: 4214 ) // warning C4214: nonstandard extension used: bit field types other than int

#endif


#endif