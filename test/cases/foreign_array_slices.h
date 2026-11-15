#ifndef FOREIGN_ARRAY_SLICES_H
#define FOREIGN_ARRAY_SLICES_H
#include <stdint.h>
#include <stddef.h>
#include <__xl_runtime/xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FixtureArraySlice {
    uint32_t iSize;
    const uint8_t* data;
    uint64_t length;
    uint64_t tag;
} FixtureArraySlice;
typedef struct FixtureSliceOptions { uint32_t iSize; int value; } FixtureSliceOptions;
void fixtureSliceRemember(int,const uint8_t*,size_t);
void fixtureSliceRememberStorage(const xvalue*,int);
int fixtureArraySlices(const FixtureArraySlice*,uint64_t);
int fixtureArraySlicesPacked(const FixtureSliceOptions*,const FixtureArraySlice*,uint64_t);
int fixtureArraySlicesReenter(const FixtureArraySlice*,uint64_t,int(*)(void*),void*);
int fixtureSliceCalls(void);
void fixtureSliceFailure(int);
void fixtureSliceHeapBegin(void);
void fixtureSliceBudget(int);
void fixtureSliceEndBudget(void);
void fixtureSliceHeapBalanced(void);
void fixtureSliceReport(int);
void fixtureSliceNativeHot(void);
#ifdef __cplusplus
}
#endif
#endif
