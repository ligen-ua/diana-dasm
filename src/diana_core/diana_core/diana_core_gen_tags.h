#ifndef DIANA_CORE_GEN_TAGS_H
#define DIANA_CORE_GEN_TAGS_H


#include "diana_core.h"

// forward declaration
struct _dianaProcessor;

typedef int (*DianaProcessorCommand_type)(struct _dianaContext * pDianaContext,
                                          struct _dianaProcessor * pCallContext);


// NOTE: this macros will be changed if more that one processer will appear
#define DIANA_QUERY_PROCESSOR_TAG(pGroupInfo) \
    ((DianaProcessorCommand_type)((pGroupInfo)->m_pTag))

#define DIANA_SET_PROCESSOR_TAG(pGroupInfo, pTag) \
    ((pGroupInfo)->m_pTag) = (pTag)

#endif