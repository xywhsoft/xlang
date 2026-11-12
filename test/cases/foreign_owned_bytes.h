#ifndef FOREIGN_OWNED_BYTES_H
#define FOREIGN_OWNED_BYTES_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int fixtureOwnedBytes(int mode,int fail_after,void** data,size_t* size);
int fixtureOwnedChars(char** data,int marker,uint64_t* size);
int fixtureOwnedUChars(unsigned char** data,size_t* size);
int fixtureOwnedPrefix(const unsigned char* input,size_t count,void** data,size_t* size);
int fixtureOwnedBytesIdentity(const unsigned char* data,size_t size);
void fixtureOwnedBytesReset(void);
void fixtureOwnedBytesBegin(void);
void fixtureOwnedBytesEnd(void);
void fixtureOwnedBytesNativeReference(void);
void fixtureOwnedBytesLanguageReference(void);
#ifdef __cplusplus
}
#endif
#endif
