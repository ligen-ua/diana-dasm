#ifndef DIANA_ELF_ANALYZER_H
#define DIANA_ELF_ANALYZER_H

#include "diana_elf.h"
#include "diana_analyze.h"

#ifdef __cplusplus
extern "C" {
#endif

int Diana_ELF_AnalyzeELF(Diana_ElfFile * pElfFile,
                          DianaAnalyzeObserver * pObserver,
                          Diana_InstructionsOwner * pOwner,
                          int analyserFlags);

#ifdef __cplusplus
}
#endif

#endif /* DIANA_ELF_ANALYZER_H */
