#ifndef XL_API_PATH_H
#define XL_API_PATH_H

#include <stddef.h>

char* xlDupText(const char* text);
int xlPathIsAbsolute(const char* path);
int xlCopyToBuffer(char* out, size_t outSize, const char* text);
const char* xlBaseName(const char* path);
int xlTextEqualsNoCase(const char* a, const char* b);
void xlDirName(char* out, size_t outSize, const char* path);
int xlJoinPathBuffer(char* out, size_t outSize, const char* dir, const char* name);
int xlGetModuleDir(char* out, size_t outSize);
int xlGetProjectRoot(char* out, size_t outSize);
int xlTextLooksLikePath(const char* text);
char* xlPathSnapshot(const char* baseDir, const char* path);
char* xlToolchainPathSnapshot(const char* baseDir, const char* path);
char* xlDefaultTempDirSnapshot(const char* baseDir);

#endif
