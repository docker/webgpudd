#ifndef WEBGPUDD_H
#define WEBGPUDD_H

#include <dawn/webgpu.h>

int initWebGPUDD();
WGPUInstance getWebGPUDDInstance();
int finaliseWebGPUDD();
int webGPUDDFlush();
void webGPUDDSetDefaultDeviceCallbacks(WGPUDevice device);

#endif /* WEBGPUDD_H */
