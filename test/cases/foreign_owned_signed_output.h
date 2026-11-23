#ifndef FOREIGN_OWNED_SIGNED_OUTPUT_H
#define FOREIGN_OWNED_SIGNED_OUTPUT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int signedOutputBytes(int mode,void** data,int* count);
int signedOutputText(int mode,char** data,int* count);
int signedOutputPair(int mode,char** data,int* count,int* scalar);
int signedOutputBytesIdentity(const unsigned char* data,size_t count);
int signedOutputTextIdentity(const char* data,size_t count);
unsigned signedOutputCalls(void);
void signedOutputReset(void);
void signedOutputBegin(void);
void signedOutputEnd(void);
#ifdef __cplusplus
}
#endif
#endif
