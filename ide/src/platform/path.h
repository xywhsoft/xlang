#ifndef XLANG_IDE_PATH_H
#define XLANG_IDE_PATH_H
#include <stddef.h>
/* Returned strings use malloc/free. Paths are absolute UTF-8 with '/' separators. */
char* xideCreateId(const char* prefix);
/* An explicit base supplies the current directory/volume for ordinary,
   same-drive-relative and volume-root-relative paths. Other drives use their
   inherited =X: environment entry, or the drive root. No CWD changes. */
char* xidePathResolve(const char* baseDirectory, const char* path);
/* Preserve requested spelling for physical/case-only renames. */
char* xidePathResolveLexical(const char* baseDirectory,const char* path);
/* Absolute extended Win32 paths for I/O, with backslashes. malloc/free;
   lexical conversion only: callers still validate/pin ordinary files. */
wchar_t* xidePathNativeWide(const char* path);
char* xidePathNativeUtf8(const char* path);
char* xidePathDirectory(const char* absolutePath);
char* xidePathRelative(const char* baseDirectory, const char* absolutePath);
const char* xidePathFilename(const char* path);
int xidePathEqual(const char* left, const char* right);
/* Compiler overlays use absolute lexical, Unicode case-insensitive paths. */
int xidePathLexicalEqual(const char* left,const char* right);
int xidePathExists(const char* path);
int xidePathIsDirectory(const char* path);
int xidePathHasExtension(const char* path, const char* extension);
#endif
