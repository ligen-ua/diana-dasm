#ifndef DIANA_WIN32_EHOOK_H
#define DIANA_WIN32_EHOOK_H

int DianaWin32Processor_EhookSTD(void * pPlaceToHook,
                                 int argSize,
                                 int defaultErrorCode);

#endif