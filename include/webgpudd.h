#ifndef WEBGPUDD_H
#define WEBGPUDD_H

#include "webgpu.h"

#ifdef __cplusplus
extern "C" {
#endif

int initWebGPUDD();
WGPUInstance getWebGPUDDInstance();
int finaliseWebGPUDD();
int webGPUDDFlush();

#ifdef __cplusplus
}
#endif

#endif /* WEBGPUDD_H */
