#ifndef FOREIGN_OWNED_TEXT_H
#define FOREIGN_OWNED_TEXT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int fixtureOwnedText(int mode,char** data,uint64_t* size);
int fixtureOwnedTextIdentity(const char* data,size_t size);
int fixtureOwnedTextCString(const char* data);
typedef struct fixture_owned_options { int marker; } fixture_owned_options;
int fixtureOwnedTextFromSpan(const fixture_owned_options* options,const char* data,uint64_t count,char** output,uint64_t* size);
void fixtureOwnedTextReset(void);
void fixtureOwnedTextBegin(void);
void fixtureOwnedTextEnd(void);
void fixtureOwnedTextNativeReference(void);
void fixtureOwnedTextLanguageReference(void);
#ifdef __cplusplus
}
#endif
#endif
