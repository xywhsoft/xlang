#ifndef FOREIGN_XGE3_GPU_BUFFER_ZEROED_H
#define FOREIGN_XGE3_GPU_BUFFER_ZEROED_H

#include <xge3_runtime.h>

bool fixtureGpuBufferContents(xge_buffer buffer,int lastByte,bool uploaded);
void fixtureGpuBufferStartMeasure(void);
void fixtureGpuBufferFinishMeasure(int language);
void fixtureGpuBufferMeasureC(void);

#endif
