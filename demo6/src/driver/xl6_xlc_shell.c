#include "include/xlc.h"
#include "src/common/xl6_xrt.h"
#include "src/common/xl6_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
#define XLC_SHELL_PATH_SEPARATOR "\\"
#else
#define XLC_SHELL_PATH_SEPARATOR "/"
#endif

static char xlcShellKeptTempDir[512];
static char xlcShellProjectKind[16];

static void xlcShellPrintHelp(void)
{
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "Usage:\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl lex <file.xl> [--format text|json] [--emit output.tokens]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl parse <file.xl> [--format tree|json] [--emit output.ast]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl check <file.xl> [--json]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl ir <file.xl> [--lowered] [--format text|json] [--emit output.xlir]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl cgen <file.xl> [--backend xlir] [--emit output.c] [--emit-header output.h] [--dump-dir dir]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl abi <file.xl> [--json] [--emit output.abi.txt]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl run <file.xl|file.c|file.xlp> [helper.c ...] [--backend xlir] [--save-temps] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib library] [-- <args>...]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl build <file.xl|file.c|file.xlp> [helper.c ...] [-o output] [-shared] [--backend xlir] [--res file.json|file.xlon] [--save-temps] [--emit-header output.h] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib library]\n");
#ifdef _WIN32
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl impdef <file.dll> -o <output.def>\n");
#endif
}

static const char* xlcShellFileArg(int argc, char** argv)
{
    return argc >= 3 ? argv[2] : NULL;
}

static int xlcShellTextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static int xlcShellHasJson(int argc, char** argv)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--json") ) {
            return 1;
        }
        if ( xlcShellTextEquals(argv[i], "--format") && i + 1 < argc && xlcShellTextEquals(argv[i + 1], "json") ) {
            return 1;
        }
    }
    return 0;
}

static int xlcShellHasFlag(int argc, char** argv, const char* flag)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--") ) {
            break;
        }
        if ( xlcShellTextEquals(argv[i], flag) ) {
            return 1;
        }
    }
    return 0;
}

static const char* xlcShellFindValue(int argc, char** argv, const char* option, int start)
{
    int i;

    for ( i = start; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--") ) {
            break;
        }
        if ( xlcShellTextEquals(argv[i], option) && i + 1 < argc ) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static int xlcShellFindArgSplit(int argc, char** argv)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--") ) {
            return i + 1;
        }
    }
    return argc;
}

static int xlcShellMakeDir(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    return xrtDirCreateAll((str)path) != 0;
}

static int xlcShellCopyText(char* out, size_t outSize, const char* text)
{
    size_t len;

    if ( out == NULL || outSize == 0 || text == NULL ) {
        return 0;
    }
    len = strlen(text);
    if ( len >= outSize ) {
        return 0;
    }
    memcpy(out, text, len + 1u);
    return 1;
}

static void xlcShellDirNameInPlace(char* path)
{
    char* slash;
    char* backslash;
    char* cut;

    if ( path == NULL || path[0] == '\0' ) {
        return;
    }
    slash = strrchr(path, '/');
    backslash = strrchr(path, '\\');
    cut = slash != NULL && (backslash == NULL || slash > backslash) ? slash : backslash;
    if ( cut == NULL ) {
        path[0] = '\0';
        return;
    }
    if ( cut == path ) {
        cut[1] = '\0';
        return;
    }
    *cut = '\0';
}

static int xlcShellGetExeDir(char* out, size_t outSize)
{
    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    out[0] = '\0';
#ifdef _WIN32
    {
        wchar_t wide[32768];
        DWORD len;
        char* utf8;
        int ok;

        len = GetModuleFileNameW(NULL, wide, (DWORD)(sizeof(wide) / sizeof(wide[0])));
        if ( len == 0 || len >= (DWORD)(sizeof(wide) / sizeof(wide[0])) ) {
            return 0;
        }
        wide[len] = L'\0';
        utf8 = xrtUtf16To8((const uint16*)wide, NULL);
        if ( utf8 == NULL ) {
            return 0;
        }
        ok = xlcShellCopyText(out, outSize, utf8);
        xrtFree(utf8);
        if ( !ok ) {
            out[0] = '\0';
            return 0;
        }
    }
#else
    {
        char path[4096];
        ssize_t len;

        len = readlink("/proc/self/exe", path, sizeof(path) - 1u);
        if ( len <= 0 || (size_t)len >= sizeof(path) ) {
            return 0;
        }
        path[len] = '\0';
        if ( !xlcShellCopyText(out, outSize, path) ) {
            return 0;
        }
    }
#endif
    xlcShellDirNameInPlace(out);
    return out[0] != '\0';
}

static int xlcShellJoinDirPath(char* out, size_t outSize, const char* dir, const char* name)
{
    const char* sep;
    size_t len;
    int written;

    if ( out == NULL || outSize == 0 || dir == NULL || name == NULL ) {
        return 0;
    }
    len = strlen(dir);
    sep = (len > 0 && (dir[len - 1u] == '\\' || dir[len - 1u] == '/')) ?
          "" :
          XLC_SHELL_PATH_SEPARATOR;
    written = snprintf(out, outSize, "%s%s%s", dir, sep, name);
    return written > 0 && (size_t)written < outSize;
}

static int xlcShellBuildTmpDir(char* out, size_t outSize)
{
    char exeDir[512];

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    if ( !xlcShellGetExeDir(exeDir, sizeof(exeDir)) ||
         !xlcShellJoinDirPath(out, outSize, exeDir, "tmp") ) {
        return 0;
    }
    /* 计算默认路径不创建目录；只有显式保存或导出文件时才落盘。 */
    return 1;
}

static int xlcShellDefaultTempDir(char* out, size_t outSize)
{
    char tmpDir[512];

    if ( !xlcShellBuildTmpDir(tmpDir, sizeof(tmpDir)) ) {
        return 0;
    }
    return xlcShellJoinDirPath(out, outSize, tmpDir, "xlc_shell");
}

static unsigned long xlcShellProcessId(void)
{
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static unsigned long xlcShellTick(void)
{
    return (unsigned long)xrtNow();
}

static int xlcShellMakeTaggedTempDir(char* out, size_t outSize, const char* tag)
{
    unsigned long pid;
    unsigned long tick;
    char tmpDir[512];
    int i;

    if ( out == NULL || outSize == 0 || tag == NULL ) {
        return 0;
    }
    if ( !xlcShellBuildTmpDir(tmpDir, sizeof(tmpDir)) ) {
        return 0;
    }
    pid = xlcShellProcessId();
    tick = xlcShellTick();
    for ( i = 0; i < 100; ++i ) {
        int written = snprintf(
            out,
            outSize,
            "%s%sxl6_%s_%lu_%lu_%d",
            tmpDir,
            XLC_SHELL_PATH_SEPARATOR,
            tag,
            pid,
            tick,
            i);
        if ( written <= 0 || (size_t)written >= outSize ) {
            return 0;
        }
        if ( xlcShellMakeDir(out) ) {
            return 1;
        }
    }
    return 0;
}

static const char* xlcShellBaseName(const char* path)
{
    const char* lastSlash;
    const char* lastBackslash;

    if ( path == NULL ) {
        return "";
    }
    lastSlash = strrchr(path, '/');
    lastBackslash = strrchr(path, '\\');
    if ( lastBackslash != NULL && (lastSlash == NULL || lastBackslash > lastSlash) ) {
        lastSlash = lastBackslash;
    }
    return lastSlash != NULL ? lastSlash + 1 : path;
}

static char xlcShellHeaderGuardChar(char ch)
{
    if ( ch >= 'a' && ch <= 'z' ) {
        return (char)(ch - 'a' + 'A');
    }
    if ( (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ) {
        return ch;
    }
    return '_';
}

static void xlcShellMakeHeaderGuard(char* out, size_t outSize, const char* path)
{
    const char* base;
    const char* p;
    const char* dot;
    size_t used;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    snprintf(out, outSize, "XL6_GENERATED_HEADER_H");
    if ( path == NULL || path[0] == '\0' ) {
        return;
    }
    base = xlcShellBaseName(path);
    dot = strrchr(base, '.');
    if ( dot == base || base[0] == '\0' ) {
        return;
    }
    snprintf(out, outSize, "XL6_");
    used = strlen(out);
    for ( p = base; *p != '\0' && p != dot && used + 3 < outSize; ++p ) {
        out[used++] = xlcShellHeaderGuardChar(*p);
    }
    if ( used + 3 < outSize ) {
        out[used++] = '_';
        out[used++] = 'H';
    }
    out[used] = '\0';
}

static int xlcShellEmitHeader(XLC_Context* ctx, const char* outputPath)
{
    char guard[256];

    xlcShellMakeHeaderGuard(guard, sizeof(guard), outputPath);
    return xlcEmitHeader(ctx, outputPath, guard);
}

static int xlcShellEmitXlirHeader(XLC_Context* ctx, const char* outputPath)
{
    char guard[256];
    xlcShellMakeHeaderGuard(guard, sizeof(guard), outputPath);
    return xlcEmitXlirHeader(ctx, outputPath, guard);
}

static int xlcShellTextEqualsNoCase(const char* a, const char* b)
{
    unsigned char ca;
    unsigned char cb;

    if ( a == NULL || b == NULL ) {
        return 0;
    }
    while ( *a != '\0' && *b != '\0' ) {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if ( ca >= 'A' && ca <= 'Z' ) {
            ca = (unsigned char)(ca - 'A' + 'a');
        }
        if ( cb >= 'A' && cb <= 'Z' ) {
            cb = (unsigned char)(cb - 'A' + 'a');
        }
        if ( ca != cb ) {
            return 0;
        }
    }
    return *a == '\0' && *b == '\0';
}

static int xlcShellHasExtensionNoCase(const char* path, const char* ext)
{
    const char* base;
    const char* dot;

    if ( path == NULL || ext == NULL ) {
        return 0;
    }
    base = xlcShellBaseName(path);
    dot = strrchr(base, '.');
    return dot != NULL && xlcShellTextEqualsNoCase(dot, ext);
}

static int xlcShellIsCSourcePath(const char* path)
{
    return xlcShellHasExtensionNoCase(path, ".c") ||
           xlcShellHasExtensionNoCase(path, ".cc") ||
           xlcShellHasExtensionNoCase(path, ".cpp") ||
           xlcShellHasExtensionNoCase(path, ".cxx");
}

static int xlcShellIsXlangSourcePath(const char* path)
{
    return xlcShellHasExtensionNoCase(path, ".xl");
}

static int xlcShellIsProjectPath(const char* path)
{
    return xlcShellHasExtensionNoCase(path, ".xlp");
}

static int xlcShellIsResourceConfigPath(const char* path)
{
    return xlcShellHasExtensionNoCase(path, ".json") ||
           xlcShellHasExtensionNoCase(path, ".xlon");
}

typedef struct XLC_ShellJson {
    const char* text;
    size_t len;
    size_t pos;
} XLC_ShellJson;

typedef struct XLC_ShellStringList {
    char** items;
    size_t count;
} XLC_ShellStringList;

typedef struct XLC_ShellProject {
    char* format;
    char* kind;
    char* entry;
    char dir[512];
    XLC_ShellStringList cSources;
    XLC_ShellStringList resources;
} XLC_ShellProject;

static char* xlcShellDupText(const char* text)
{
    const char* src;

    src = text != NULL ? text : "";
    return xrtStrDupN(src, strlen(src));
}

static void xlcShellStringListInit(XLC_ShellStringList* list)
{
    if ( list != NULL ) {
        memset(list, 0, sizeof(*list));
    }
}

static void xlcShellStringListFree(XLC_ShellStringList* list)
{
    size_t i;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < list->count; ++i ) {
        xrtFree(list->items[i]);
    }
    xrtFree(list->items);
    memset(list, 0, sizeof(*list));
}

static int xlcShellStringListAppend(XLC_ShellStringList* list, const char* text)
{
    char** newItems;
    char* copy;

    if ( list == NULL || text == NULL ) {
        return 0;
    }
    copy = xlcShellDupText(text);
    if ( copy == NULL ) {
        return 0;
    }
    newItems = (char**)xrtRealloc(list->items, (list->count + 1u) * sizeof(char*));
    if ( newItems == NULL ) {
        xrtFree(copy);
        return 0;
    }
    list->items = newItems;
    list->items[list->count] = copy;
    list->count += 1;
    return 1;
}

static void xlcShellProjectInit(XLC_ShellProject* project)
{
    if ( project != NULL ) {
        memset(project, 0, sizeof(*project));
        xlcShellStringListInit(&project->cSources);
        xlcShellStringListInit(&project->resources);
    }
}

static void xlcShellProjectFree(XLC_ShellProject* project)
{
    if ( project == NULL ) {
        return;
    }
    xrtFree(project->format);
    xrtFree(project->kind);
    xrtFree(project->entry);
    xlcShellStringListFree(&project->cSources);
    xlcShellStringListFree(&project->resources);
    memset(project, 0, sizeof(*project));
}

static void xlcShellJsonSkipWs(XLC_ShellJson* json)
{
    while ( json != NULL && json->pos < json->len ) {
        char ch = json->text[json->pos];
        if ( ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' ) {
            break;
        }
        json->pos += 1;
    }
}

static int xlcShellJsonPeek(XLC_ShellJson* json)
{
    xlcShellJsonSkipWs(json);
    if ( json == NULL || json->pos >= json->len ) {
        return 0;
    }
    return (unsigned char)json->text[json->pos];
}

static int xlcShellJsonConsume(XLC_ShellJson* json, char ch)
{
    xlcShellJsonSkipWs(json);
    if ( json == NULL || json->pos >= json->len || json->text[json->pos] != ch ) {
        return 0;
    }
    json->pos += 1;
    return 1;
}

static int xlcShellJsonAppendChar(char** out, size_t* len, size_t* cap, char ch)
{
    char* newData;
    size_t newCap;

    if ( out == NULL || len == NULL || cap == NULL ) {
        return 0;
    }
    if ( *len + 2 > *cap ) {
        newCap = *cap == 0 ? 32 : *cap * 2;
        while ( *len + 2 > newCap ) {
            newCap *= 2;
        }
        newData = (char*)xrtRealloc(*out, newCap);
        if ( newData == NULL ) {
            return 0;
        }
        *out = newData;
        *cap = newCap;
    }
    (*out)[*len] = ch;
    *len += 1;
    (*out)[*len] = '\0';
    return 1;
}

static char* xlcShellJsonReadString(XLC_ShellJson* json)
{
    char* out;
    size_t len;
    size_t cap;

    if ( !xlcShellJsonConsume(json, '"') ) {
        return NULL;
    }
    out = NULL;
    len = 0;
    cap = 0;
    while ( json->pos < json->len ) {
        char ch = json->text[json->pos++];
        if ( ch == '"' ) {
            return out != NULL ? out : xlcShellDupText("");
        }
        if ( ch == '\\' && json->pos < json->len ) {
            ch = json->text[json->pos++];
            switch ( ch ) {
            case '"': ch = '"'; break;
            case '\\': ch = '\\'; break;
            case '/': ch = '/'; break;
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case 'u':
                if ( json->pos + 4 <= json->len ) {
                    json->pos += 4;
                }
                ch = '?';
                break;
            default:
                break;
            }
        }
        if ( !xlcShellJsonAppendChar(&out, &len, &cap, ch) ) {
            xrtFree(out);
            return NULL;
        }
    }
    xrtFree(out);
    return NULL;
}

static int xlcShellJsonReadBool(XLC_ShellJson* json)
{
    xlcShellJsonSkipWs(json);
    if ( json->pos + 4 <= json->len && memcmp(json->text + json->pos, "true", 4) == 0 ) {
        json->pos += 4;
        return 1;
    }
    if ( json->pos + 5 <= json->len && memcmp(json->text + json->pos, "false", 5) == 0 ) {
        json->pos += 5;
        return 1;
    }
    return 0;
}

static int xlcShellJsonSkipValue(XLC_ShellJson* json);

static int xlcShellJsonSkipObject(XLC_ShellJson* json)
{
    if ( !xlcShellJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xlcShellJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xlcShellJsonReadString(json);
        if ( key == NULL ) {
            return 0;
        }
        xrtFree(key);
        if ( !xlcShellJsonConsume(json, ':') || !xlcShellJsonSkipValue(json) ) {
            return 0;
        }
        if ( xlcShellJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xlcShellJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xlcShellJsonSkipArray(XLC_ShellJson* json)
{
    if ( !xlcShellJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xlcShellJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        if ( !xlcShellJsonSkipValue(json) ) {
            return 0;
        }
        if ( xlcShellJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xlcShellJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xlcShellJsonSkipValue(XLC_ShellJson* json)
{
    int ch;

    ch = xlcShellJsonPeek(json);
    if ( ch == '"' ) {
        char* text = xlcShellJsonReadString(json);
        xrtFree(text);
        return text != NULL;
    }
    if ( ch == '{' ) {
        return xlcShellJsonSkipObject(json);
    }
    if ( ch == '[' ) {
        return xlcShellJsonSkipArray(json);
    }
    if ( ch == 't' || ch == 'f' ) {
        return xlcShellJsonReadBool(json);
    }
    if ( ch == 'n' && json->pos + 4 <= json->len && memcmp(json->text + json->pos, "null", 4) == 0 ) {
        json->pos += 4;
        return 1;
    }
    xlcShellJsonSkipWs(json);
    while ( json->pos < json->len ) {
        ch = (unsigned char)json->text[json->pos];
        if ( ch == ',' || ch == ']' || ch == '}' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ) {
            break;
        }
        json->pos += 1;
    }
    return 1;
}

static int xlcShellJsonReadStringArray(XLC_ShellJson* json, XLC_ShellStringList* list)
{
    if ( !xlcShellJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xlcShellJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        char* text = xlcShellJsonReadString(json);
        int ok = text != NULL && xlcShellStringListAppend(list, text);
        xrtFree(text);
        if ( !ok ) {
            return 0;
        }
        if ( xlcShellJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xlcShellJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xlcShellProjectParseRoot(XLC_ShellJson* json, XLC_ShellProject* project)
{
    if ( !xlcShellJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xlcShellJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xlcShellJsonReadString(json);
        if ( key == NULL || !xlcShellJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "format") == 0 ) {
            xrtFree(project->format);
            project->format = xlcShellJsonReadString(json);
        } else if ( strcmp(key, "kind") == 0 ) {
            xrtFree(project->kind);
            project->kind = xlcShellJsonReadString(json);
        } else if ( strcmp(key, "entry") == 0 ) {
            xrtFree(project->entry);
            project->entry = xlcShellJsonReadString(json);
        } else if ( strcmp(key, "cSources") == 0 ) {
            if ( !xlcShellJsonReadStringArray(json, &project->cSources) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "resources") == 0 ) {
            if ( !xlcShellJsonReadStringArray(json, &project->resources) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( !xlcShellJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xlcShellJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xlcShellJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xlcShellProjectIsSharedKind(const char* kind)
{
    return xlcShellTextEqualsNoCase(kind, "dll") || xlcShellTextEqualsNoCase(kind, "shared");
}

static int xlcShellProjectValidate(const char* path, const XLC_ShellProject* project)
{
    size_t i;

    if ( project->format != NULL && !xlcShellTextEquals(project->format, "xlang-project-1") ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "invalid xlp format: %s\n", path);
        return 0;
    }
    if ( project->kind == NULL || project->kind[0] == '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "xlp requires kind: %s\n", path);
        return 0;
    }
    if ( !xlcShellTextEqualsNoCase(project->kind, "exe") && !xlcShellProjectIsSharedKind(project->kind) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "xlp kind must be exe or dll: %s\n", path);
        return 0;
    }
    if ( project->entry == NULL || project->entry[0] == '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "xlp requires entry: %s\n", path);
        return 0;
    }
    if ( !xlcShellIsXlangSourcePath(project->entry) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "xlp entry must be an .xl file: %s\n", project->entry);
        return 0;
    }
    for ( i = 0; i < project->cSources.count; ++i ) {
        if ( !xlcShellIsCSourcePath(project->cSources.items[i]) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "xlp cSources only accepts C source files: %s\n", project->cSources.items[i]);
            return 0;
        }
    }
    for ( i = 0; i < project->resources.count; ++i ) {
        if ( !xlcShellIsResourceConfigPath(project->resources.items[i]) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "xlp resources only accepts json/xlon resource configs: %s\n", project->resources.items[i]);
            return 0;
        }
    }
    return 1;
}

static int xlcShellIsAbsolutePath(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    if ( path[0] == '\\' || path[0] == '/' ) {
        return 1;
    }
    return ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
           path[1] == ':';
}

static int xlcShellProjectResolvePath(char* out, size_t outSize, const XLC_ShellProject* project, const char* path)
{
    if ( out == NULL || outSize == 0 || project == NULL || path == NULL ) {
        return 0;
    }
    if ( xlcShellIsAbsolutePath(path) ) {
        return xlcShellCopyText(out, outSize, path);
    }
    return xlcShellJoinDirPath(out, outSize, project->dir[0] != '\0' ? project->dir : ".", path);
}

static int xlcShellProjectLoad(const char* path, XLC_ShellProject* project)
{
    char* text;
    size_t textLen;
    XLC_ShellJson json;
    int ok;

    if ( path == NULL || project == NULL ) {
        return 0;
    }
    xlcShellProjectFree(project);
    xlcShellProjectInit(project);
    textLen = 0;
    text = xrtFileReadText(path, XENCODING_UTF8, XUTF_STRICT, &textLen);
    if ( text == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot read xlp: %s\n", path);
        return 0;
    }
    memset(&json, 0, sizeof(json));
    json.text = text;
    json.len = textLen;
    if ( json.len >= 3 &&
         (unsigned char)json.text[0] == 0xEF &&
         (unsigned char)json.text[1] == 0xBB &&
         (unsigned char)json.text[2] == 0xBF ) {
        json.pos = 3;
    }
    ok = xlcShellProjectParseRoot(&json, project);
    xrtFree(text);
    if ( !ok || !xlcShellProjectValidate(path, project) ) {
        if ( ok ) {
            xlcShellProjectFree(project);
        } else {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "invalid xlp json: %s\n", path);
            xlcShellProjectFree(project);
        }
        return 0;
    }
    if ( !xlcShellCopyText(project->dir, sizeof(project->dir), path) ) {
        xlcShellProjectFree(project);
        return 0;
    }
    xlcShellDirNameInPlace(project->dir);
    if ( project->dir[0] == '\0' ) {
        if ( !xlcShellCopyText(project->dir, sizeof(project->dir), ".") ) {
            xlcShellProjectFree(project);
            return 0;
        }
    }
    return 1;
}

static int xlcShellOptionTakesValue(const char* arg);

/* 与 DLL 编译 API 保持一致，允许直接编译官方库的完整源文件清单。 */
#define XLC_SHELL_MAX_C_INPUTS XLC_MAX_C_SOURCE_FILES

static int xlcShellCollectCInputs(int argc, char** argv, const char** inputs, int maxInputs, int* outCount)
{
    int i;
    int count;

    if ( inputs == NULL || outCount == NULL || maxInputs <= 0 ) {
        return 0;
    }
    count = 0;
    for ( i = 2; i < argc; ++i ) {
        const char* arg = argv[i];

        if ( xlcShellTextEquals(arg, "--") ) {
            break;
        }
        if ( xlcShellOptionTakesValue(arg) ) {
            i += 1;
            continue;
        }
        if ( arg == NULL || arg[0] == '-' ) {
            continue;
        }
        if ( xlcShellIsCSourcePath(arg) ) {
            if ( count >= maxInputs ) {
                xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many C source files\n");
                return 0;
            }
            inputs[count++] = arg;
        }
    }
    if ( count <= 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "missing C source file\n");
        return 0;
    }
    *outCount = count;
    return 1;
}

static int xlcShellIsDefaultRuntimeLink(const char* value)
{
    const char* base;

    if ( value == NULL ) {
        return 0;
    }
    base = xlcShellBaseName(value);
    return xlcShellTextEqualsNoCase(value, "xrt") ||
           xlcShellTextEqualsNoCase(base, "xrt.o") ||
           xlcShellTextEqualsNoCase(base, "xrt.obj") ||
           xlcShellTextEqualsNoCase(base, "xrt.lib") ||
           xlcShellTextEqualsNoCase(base, "libxrt.a") ||
           xlcShellTextEqualsNoCase(base, "libtcc1.a") ||
           xlcShellTextEqualsNoCase(base, "libtcc1-64.a");
}

static void xlcShellBaseNoExt(char* out, size_t outSize, const char* path)
{
    const char* base;
    const char* dot;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    base = xlcShellBaseName(path);
    dot = strrchr(base, '.');
    len = dot != NULL && dot > base ? (size_t)(dot - base) : strlen(base);
    if ( len >= outSize ) {
        len = outSize - 1;
    }
    memcpy(out, base, len);
    out[len] = '\0';
}

static int xlcShellJoinPath(char* out, size_t outSize, const char* dir, const char* base, const char* suffix)
{
    int written;

    if ( out == NULL || outSize == 0 || dir == NULL || base == NULL || suffix == NULL ) {
        return 0;
    }
    written = snprintf(
        out,
        outSize,
        "%s%s%s%s",
        dir,
        XLC_SHELL_PATH_SEPARATOR,
        base,
        suffix);
    return written > 0 && (size_t)written < outSize;
}

static const char* xlcShellDefaultExeSuffix(void)
{
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
}

static int xlcShellDefaultBuildOutput(char* out, size_t outSize, const char* inputPath)
{
    char base[256];
    const char* suffix;
    int written;

    if ( out == NULL || outSize == 0 || inputPath == NULL || inputPath[0] == '\0' ) {
        return 0;
    }
    xlcShellBaseNoExt(base, sizeof(base), inputPath);
    if ( base[0] == '\0' ) {
        return 0;
    }
    suffix = xlcShellDefaultExeSuffix();
    written = snprintf(out, outSize, "%s%s", base, suffix);
    return written > 0 && (size_t)written < outSize;
}

static int xlcShellOptionTakesValue(const char* arg)
{
    return xlcShellTextEquals(arg, "--emit") ||
           xlcShellTextEquals(arg, "--backend") ||
           xlcShellTextEquals(arg, "--emit-header") ||
           xlcShellTextEquals(arg, "--dump-dir") ||
           xlcShellTextEquals(arg, "--format") ||
           xlcShellTextEquals(arg, "--inc-path") ||
           xlcShellTextEquals(arg, "--lib-path") ||
           xlcShellTextEquals(arg, "--link") ||
           xlcShellTextEquals(arg, "--res") ||
           xlcShellTextEquals(arg, "--loadlib") ||
           xlcShellTextEquals(arg, "--break") ||
           xlcShellTextEquals(arg, "--commands") ||
           xlcShellTextEquals(arg, "-o");
}

static int xlcShellApplyContextOptions(XLC_Context* ctx, int argc, char** argv)
{
    const char* backend;
    int i;

    if ( ctx == NULL ) {
        return 0;
    }
    if ( xlcShellHasFlag(argc, argv, "--save-temps") ) {
        if ( !xlcSetSaveTemps(ctx, 1) ) {
            return 0;
        }
    }
    backend = xlcShellFindValue(argc, argv, "--backend", 3);
    if ( backend != NULL ) {
        if ( xlcShellTextEquals(backend, "xlir") ) {
            if ( !xlcSetBackend(ctx, XLC_BACKEND_XLIR) ) return 0;
        } else {
            xl6ConsolePuts(XL6_CONSOLE_STDERR, "only the xlir backend is available\n");
            return 0;
        }
    }
    for ( i = 3; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--") ) {
            break;
        }
        if ( i + 1 < argc && xlcShellTextEquals(argv[i], "--inc-path") ) {
            if ( !xlcAddIncludePath(ctx, argv[++i]) ) {
                return 0;
            }
        } else if ( i + 1 < argc && xlcShellTextEquals(argv[i], "--lib-path") ) {
            if ( !xlcAddLibraryPath(ctx, argv[++i]) ) {
                return 0;
            }
        } else if ( i + 1 < argc && xlcShellTextEquals(argv[i], "--link") ) {
            i += 1;
            if ( xlcShellIsDefaultRuntimeLink(argv[i]) ) {
                continue;
            }
            if ( !xlcAddLinkLibrary(ctx, argv[i]) ) {
                return 0;
            }
        } else if ( i + 1 < argc && xlcShellTextEquals(argv[i], "--res") ) {
            if ( !xlcAddResourceFile(ctx, argv[++i]) ) {
                return 0;
            }
        } else if ( i + 1 < argc && xlcShellTextEquals(argv[i], "--loadlib") ) {
            if ( !xlcLoadDependencyLibrary(ctx, argv[++i]) ) {
                return 0;
            }
        } else if ( i + 1 < argc && (xlcShellTextEquals(argv[i], "--emit") ||
                                    xlcShellTextEquals(argv[i], "--backend") ||
                                    xlcShellTextEquals(argv[i], "--emit-header") ||
                                    xlcShellTextEquals(argv[i], "--dump-dir") ||
                                    xlcShellTextEquals(argv[i], "--format") ||
                                    xlcShellTextEquals(argv[i], "-o")) ) {
            i += 1;
        } else if ( xlcShellOptionTakesValue(argv[i]) ) {
            i += 1;
        } else if ( argv[i] != NULL && argv[i][0] != '-' ) {
            /* Extra C sources are wrapped with the generated module header; other paths stay link inputs. */
            if ( xlcShellIsCSourcePath(argv[i]) ) {
                if ( !xlcAddCSourceFile(ctx, argv[i]) ) {
                    return 0;
                }
                continue;
            }
            if ( !xlcAddLinkLibrary(ctx, argv[i]) ) {
                return 0;
            }
        }
    }
    return 1;
}

static XLC_Context* xlcShellCreateContext(int argc, char** argv)
{
    XLC_ContextOptions options;
    XLC_Context* ctx;
    char defaultTempDir[512];
    char tempDir[512];
    const char* command;

    options = xlcDefaultContextOptions();
    options.workDir = ".";
    options.tempDir = xlcShellDefaultTempDir(defaultTempDir, sizeof(defaultTempDir)) ? defaultTempDir : NULL;
    xlcShellKeptTempDir[0] = '\0';
    command = argc >= 2 ? argv[1] : "";
    if ( xlcShellHasFlag(argc, argv, "--save-temps") ) {
        const char* tag = xlcShellTextEquals(command, "run") ? "run" : "build";
        if ( !xlcShellMakeTaggedTempDir(tempDir, sizeof(tempDir), tag) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create temporary directory\n");
            return NULL;
        }
        options.tempDir = tempDir;
        snprintf(xlcShellKeptTempDir, sizeof(xlcShellKeptTempDir), "%s", tempDir);
    }
    ctx = xlcCreateContext(&options);
    if ( ctx == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create xlc context\n");
        return NULL;
    }
    if ( !xlcShellApplyContextOptions(ctx, argc, argv) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
        xlcDestroyContext(ctx);
        return NULL;
    }
    return ctx;
}

static int xlcShellAddFile(XLC_Context* ctx, const char* file)
{
    if ( file == NULL || file[0] == '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "missing input file\n");
        return 0;
    }
    if ( !xlcAddSourceFile(ctx, file) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
        return 0;
    }
    return 1;
}

static int xlcShellAddProject(XLC_Context* ctx, const char* file)
{
    XLC_ShellProject project;
    char path[512];
    size_t i;
    int ok;

    if ( ctx == NULL || file == NULL || file[0] == '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "missing xlp file\n");
        return 0;
    }
    xlcShellProjectInit(&project);
    if ( !xlcShellProjectLoad(file, &project) ) {
        xlcShellProjectFree(&project);
        return 0;
    }
    if ( !xlcShellProjectResolvePath(path, sizeof(path), &project, project.entry) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "xlp entry path is too long: %s\n", project.entry);
        xlcShellProjectFree(&project);
        return 0;
    }
    ok = xlcShellAddFile(ctx, path);
    for ( i = 0; ok && i < project.cSources.count; ++i ) {
        if ( !xlcShellProjectResolvePath(path, sizeof(path), &project, project.cSources.items[i]) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "xlp cSources path is too long: %s\n", project.cSources.items[i]);
            ok = 0;
            break;
        }
        if ( !xlcAddCSourceFile(ctx, path) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
            ok = 0;
            break;
        }
    }
    for ( i = 0; ok && i < project.resources.count; ++i ) {
        if ( !xlcShellProjectResolvePath(path, sizeof(path), &project, project.resources.items[i]) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "xlp resources path is too long: %s\n", project.resources.items[i]);
            ok = 0;
            break;
        }
        if ( !xlcAddResourceFile(ctx, path) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
            ok = 0;
            break;
        }
    }
    snprintf(xlcShellProjectKind, sizeof(xlcShellProjectKind), "%s", project.kind != NULL ? project.kind : "");
    xlcShellProjectFree(&project);
    return ok;
}

static int xlcShellAddInputFile(XLC_Context* ctx, const char* file)
{
    xlcShellProjectKind[0] = '\0';
    if ( xlcShellIsProjectPath(file) ) {
        return xlcShellAddProject(ctx, file);
    }
    return xlcShellAddFile(ctx, file);
}

static int xlcShellRunWithFileEx(int argc, char** argv, int (*proc)(XLC_Context*, int, char**), int printLastError)
{
    XLC_Context* ctx;
    int rc;

    ctx = xlcShellCreateContext(argc, argv);
    if ( ctx == NULL ) {
        return 1;
    }
    if ( !xlcShellAddInputFile(ctx, xlcShellFileArg(argc, argv)) ) {
        xlcDestroyContext(ctx);
        return 1;
    }
    rc = proc(ctx, argc, argv) ? 0 : 1;
    if ( rc != 0 && printLastError ) {
        const char* errorText = xlcGetLastError(ctx);
        if ( errorText != NULL && errorText[0] != '\0' ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", errorText);
        }
    }
    xlcDestroyContext(ctx);
    return rc;
}

static int xlcShellRunWithFile(int argc, char** argv, int (*proc)(XLC_Context*, int, char**))
{
    return xlcShellRunWithFileEx(argc, argv, proc, 1);
}

static int xlcShellDoLex(XLC_Context* ctx, int argc, char** argv)
{
    return xlcEmitTokens(ctx, xlcShellFindValue(argc, argv, "--emit", 3), xlcShellHasJson(argc, argv));
}

static int xlcShellDoParse(XLC_Context* ctx, int argc, char** argv)
{
    return xlcEmitAst(ctx, xlcShellFindValue(argc, argv, "--emit", 3), xlcShellHasJson(argc, argv));
}

static int xlcShellWriteFileToConsole(const char* path, XL6_ConsoleStream stream)
{
    char* text;
    size_t size;

    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    size = 0;
    text = (char*)xrtFileReadAll(path, &size);
    if ( text == NULL ) {
        return 0;
    }
    if ( size > 0 ) {
        (void)xl6ConsoleWrite(stream == XL6_CONSOLE_STDERR ? XL6_CONSOLE_STDERR : XL6_CONSOLE_STDOUT, text, size);
    }
    xrtFree(text);
    return 1;
}

static int xlcShellDoCheck(XLC_Context* ctx, int argc, char** argv)
{
    char tmpDir[512];
    char outputPath[512];
    int ok;

    (void)argv;
    if ( xlcShellHasJson(argc, argv) ) {
        return xlcCheck(ctx, NULL, 1);
    }
    if ( !xlcShellBuildTmpDir(tmpDir, sizeof(tmpDir)) || !xlcShellMakeDir(tmpDir) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create temporary directory\n");
        return 0;
    }
    if ( snprintf(outputPath,
                  sizeof(outputPath),
                  "%s%sxlc_check_%lu_%lu.txt",
                  tmpDir,
                  XLC_SHELL_PATH_SEPARATOR,
                  xlcShellProcessId(),
                  xlcShellTick()) <= 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create temporary check path\n");
        return 0;
    }
    ok = xlcCheck(ctx, outputPath, 0);
    if ( !xlcShellWriteFileToConsole(outputPath, ok ? XL6_CONSOLE_STDOUT : XL6_CONSOLE_STDERR) ) {
        const char* errorText = xlcGetLastError(ctx);
        if ( errorText != NULL && errorText[0] != '\0' ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", errorText);
        }
    }
    (void)xrtFileDelete((str)outputPath);
    return ok;
}

static int xlcShellDoIr(XLC_Context* ctx, int argc, char** argv)
{
    return xlcShellHasFlag(argc, argv, "--lowered")
        ? xlcEmitLoweredIr(ctx, xlcShellFindValue(argc, argv, "--emit", 3), xlcShellHasJson(argc, argv))
        : xlcEmitIr(ctx, xlcShellFindValue(argc, argv, "--emit", 3), xlcShellHasJson(argc, argv));
}

static int xlcShellDoAbi(XLC_Context* ctx, int argc, char** argv)
{
    return xlcEmitAbi(ctx, xlcShellFindValue(argc, argv, "--emit", 3), xlcShellHasJson(argc, argv));
}

static int xlcShellDoRun(XLC_Context* ctx, int argc, char** argv)
{
    char* runArgs[129];
    int runArgCount;
    int argStart;
    int i;

    if ( xlcShellProjectKind[0] != '\0' && xlcShellProjectIsSharedKind(xlcShellProjectKind) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot run dll project: %s\n", xlcShellFileArg(argc, argv));
        return 0;
    }
    runArgCount = 0;
    runArgs[runArgCount++] = (char*)xlcShellFileArg(argc, argv);
    argStart = xlcShellFindArgSplit(argc, argv);
    for ( i = argStart; i < argc; ++i ) {
        if ( runArgCount >= (int)(sizeof(runArgs) / sizeof(runArgs[0])) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many program arguments\n");
            return 0;
        }
        runArgs[runArgCount++] = argv[i];
    }
    return xlcRun(ctx, runArgCount, runArgs);
}

static int xlcShellRunRunCommand(int argc, char** argv)
{
    XLC_Context* ctx;
    int ok;
    int rc;

    ctx = xlcShellCreateContext(argc, argv);
    if ( ctx == NULL ) {
        return 1;
    }
    if ( !xlcShellAddInputFile(ctx, xlcShellFileArg(argc, argv)) ) {
        xlcDestroyContext(ctx);
        return 1;
    }
    ok = xlcShellDoRun(ctx, argc, argv);
    rc = ok ? xlcGetLastExitCode(ctx) : 1;
    if ( !ok ) {
        const char* errorText = xlcGetLastError(ctx);
        if ( errorText != NULL && errorText[0] != '\0' ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", errorText);
        }
    }
    xlcDestroyContext(ctx);
    return rc;
}

static int xlcShellRunC(int argc, char** argv)
{
    XLC_Context* ctx;
    XL_Context* runtime;
    const char* cInputs[XLC_SHELL_MAX_C_INPUTS];
    char* runArgs[129];
    int cInputCount;
    int runArgCount;
    int argStart;
    int ok;
    int rc;
    int i;

    if ( !xlcShellCollectCInputs(argc, argv, cInputs, XLC_SHELL_MAX_C_INPUTS, &cInputCount) ) {
        return 1;
    }
    ctx = xlcShellCreateContext(argc, argv);
    if ( ctx == NULL ) {
        return 1;
    }
    runtime = xlcRuntimeContext(ctx);
    runArgCount = 0;
    runArgs[runArgCount++] = (char*)cInputs[0];
    argStart = xlcShellFindArgSplit(argc, argv);
    for ( i = argStart; i < argc; ++i ) {
        if ( runArgCount >= (int)(sizeof(runArgs) / sizeof(runArgs[0])) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many program arguments\n");
            xlcDestroyContext(ctx);
            return 1;
        }
        runArgs[runArgCount++] = argv[i];
    }
    ok = runtime != NULL && xlRunCSourceFiles(runtime, cInputs, cInputCount, runArgCount, runArgs);
    rc = ok ? xlGetLastExitCode(runtime) : 1;
    if ( !ok ) {
        const char* errorText = runtime != NULL ? xlGetLastError(runtime) : xlcGetLastError(ctx);
        if ( errorText != NULL && errorText[0] != '\0' ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", errorText);
        }
    }
    xlcDestroyContext(ctx);
    return rc;
}

/* The inspection surface is derived from the single canonical XLIR pipeline. */
static int xlcShellCgenDumpDir(XLC_Context* ctx, const char* inputPath, const char* dumpDir)
{
    char base[256];
    char path[512];
    if ( dumpDir == NULL ) return 1;
    if ( !xlcShellMakeDir(dumpDir) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create dump dir: %s\n", dumpDir);
        return 0;
    }
    xlcShellBaseNoExt(base, sizeof(base), inputPath);
    if ( !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".typed.xlir") ||
         !xlcEmitIr(ctx, path, 0) ||
         !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".lowered.xlir") ||
         !xlcEmitLoweredIr(ctx, path, 0) ||
         !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".c") ||
         !xlcEmitXlirC(ctx, path) ||
         !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".h") ||
         !xlcShellEmitXlirHeader(ctx, path) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
        return 0;
    }
    return 1;
}

static int xlcShellEmitCStdout(XLC_Context* ctx)
{
    char workDir[512];
    char cPath[512];
    char* text;
    size_t size;
    int ok;

    workDir[0] = '\0';
    cPath[0] = '\0';
    text = NULL;
    ok = 0;

    if ( !xlcShellMakeTaggedTempDir(workDir, sizeof(workDir), "cgen") ||
         !xlcShellJoinPath(cPath, sizeof(cPath), workDir, "module", ".c") ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create temporary C output\n");
        goto cleanup;
    }
    if ( !xlcEmitC(ctx, cPath) ) {
        goto cleanup;
    }
    size = 0;
    text = xrtFileReadText(cPath, XENCODING_UTF8, XUTF_STRICT, &size);
    if ( text == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot read temporary C output: %s\n", cPath);
        goto cleanup;
    }
    if ( !xl6ConsoleWrite(XL6_CONSOLE_STDOUT, text, size) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot write C output to stdout\n");
        goto cleanup;
    }
    ok = 1;

cleanup:
    xrtFree(text);
    if ( cPath[0] != '\0' ) {
        (void)xrtFileDelete((str)cPath);
    }
    if ( workDir[0] != '\0' ) {
        (void)xrtDirRemove(workDir);
    }
    return ok;
}

static int xlcShellDoCgen(XLC_Context* ctx, int argc, char** argv)
{
    const char* backend;
    const char* dumpDir;
    const char* emit;
    const char* emitHeader;
    int ok;

    backend = xlcShellFindValue(argc, argv, "--backend", 3);
    if ( backend != NULL && !xlcShellTextEquals(backend, "xlir") ) {
        xl6ConsolePuts(XL6_CONSOLE_STDERR, "only the xlir backend is available\n");
        return 0;
    }
    dumpDir = xlcShellFindValue(argc, argv, "--dump-dir", 3);
    emit = xlcShellFindValue(argc, argv, "--emit", 3);
    emitHeader = xlcShellFindValue(argc, argv, "--emit-header", 3);
    if ( dumpDir != NULL && !xlcShellCgenDumpDir(
            ctx, xlcShellFileArg(argc, argv), dumpDir) ) {
        return 0;
    }
    if ( emit == NULL && emitHeader == NULL && dumpDir != NULL ) {
        return 1;
    }
    if ( emit == NULL && emitHeader == NULL ) {
        return xlcShellEmitCStdout(ctx);
    }
    ok = 1;
    if ( emit != NULL || emitHeader == NULL ) {
        ok = xlcEmitC(ctx, emit);
    }
    if ( ok && emitHeader != NULL ) {
        ok = xlcShellEmitHeader(ctx, emitHeader);
    }
    return ok;
}

static int xlcShellDoBuild(XLC_Context* ctx, int argc, char** argv)
{
    const char* output;
    const char* emitHeader;
    char defaultOutput[512];
    int cliShared;
    int buildShared;
    int ok;

    cliShared = xlcShellHasFlag(argc, argv, "-shared") || xlcShellHasFlag(argc, argv, "--shared");
    buildShared = cliShared;
    if ( xlcShellProjectKind[0] != '\0' ) {
        buildShared = xlcShellProjectIsSharedKind(xlcShellProjectKind);
        if ( cliShared && !buildShared ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "xlp kind conflicts with -shared: %s\n", xlcShellFileArg(argc, argv));
            return 0;
        }
    }
    output = xlcShellFindValue(argc, argv, "-o", 3);
    if ( output == NULL ) {
        if ( buildShared ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "build shared library requires -o <output>\n");
            return 0;
        }
        if ( !xlcShellDefaultBuildOutput(defaultOutput, sizeof(defaultOutput), xlcShellFileArg(argc, argv)) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create default output path\n");
            return 0;
        }
        output = defaultOutput;
    }
    emitHeader = xlcShellFindValue(argc, argv, "--emit-header", 3);
    if ( emitHeader != NULL && !xlcShellEmitHeader(ctx, emitHeader) ) {
        return 0;
    }
    if ( buildShared ) {
        ok = xlcEmitSharedLibrary(ctx, output);
    } else {
        ok = xlcEmitExe(ctx, output);
    }
    if ( ok && xlcShellHasFlag(argc, argv, "--save-temps") && xlcShellKeptTempDir[0] != '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "temporary files kept: %s\n", xlcShellKeptTempDir);
    }
    return ok;
}

static int xlcShellBuildC(int argc, char** argv)
{
    XLC_Context* ctx;
    XL_Context* runtime;
    const char* cInputs[XLC_SHELL_MAX_C_INPUTS];
    const char* output;
    char defaultOutput[512];
    int cInputCount;
    int buildShared;
    int ok;

    if ( !xlcShellCollectCInputs(argc, argv, cInputs, XLC_SHELL_MAX_C_INPUTS, &cInputCount) ) {
        return 1;
    }
    buildShared = xlcShellHasFlag(argc, argv, "-shared") || xlcShellHasFlag(argc, argv, "--shared");
    output = xlcShellFindValue(argc, argv, "-o", 3);
    if ( output == NULL ) {
        if ( buildShared ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "build shared library requires -o <output>\n");
            return 1;
        }
        if ( !xlcShellDefaultBuildOutput(defaultOutput, sizeof(defaultOutput), cInputs[0]) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create default output path\n");
            return 1;
        }
        output = defaultOutput;
    }
    ctx = xlcShellCreateContext(argc, argv);
    if ( ctx == NULL ) {
        return 1;
    }
    runtime = xlcRuntimeContext(ctx);
    if ( buildShared ) {
        ok = runtime != NULL && xlCompileCSourceFilesToSharedLibrary(runtime, cInputs, cInputCount, output);
    } else {
        ok = runtime != NULL && xlCompileCSourceFilesToExe(runtime, cInputs, cInputCount, output);
    }
    if ( !ok ) {
        const char* errorText = runtime != NULL ? xlGetLastError(runtime) : xlcGetLastError(ctx);
        if ( errorText != NULL && errorText[0] != '\0' ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", errorText);
        }
    }
    xlcDestroyContext(ctx);
    return ok ? 0 : 1;
}

static int xlcShellImpdef(int argc, char** argv)
{
    const char* dllPath;
    const char* output;

    dllPath = xlcShellFileArg(argc, argv);
    output = xlcShellFindValue(argc, argv, "-o", 3);
    if ( dllPath == NULL || dllPath[0] == '\0' || output == NULL || output[0] == '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "impdef requires <file.dll> -o <output.def>\n");
        return 1;
    }
    if ( !xlTccImpdef(dllPath, output) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "impdef failed: %s\n", dllPath);
        return 1;
    }
    return 0;
}

#define XLC_SHELL_MAX_RUN_ARGS 128
#define XLC_SHELL_MAX_ENV_ITEMS 4

int xlcShellMain(int argc, char** argv)
{
    const char* cmd;

    if ( argc < 2 ) {
        xlcShellPrintHelp();
        return 0;
    }
    cmd = argv[1];
    if ( xlcShellTextEquals(cmd, "help") || xlcShellTextEquals(cmd, "--help") || xlcShellTextEquals(cmd, "-h") ) {
        xlcShellPrintHelp();
        return 0;
    }
    if ( xlcShellTextEquals(cmd, "impdef") ) {
        return xlcShellImpdef(argc, argv);
    }
    if ( xlcShellTextEquals(cmd, "lex") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoLex);
    }
    if ( xlcShellTextEquals(cmd, "parse") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoParse);
    }
    if ( xlcShellTextEquals(cmd, "check") ) {
        return xlcShellRunWithFileEx(argc, argv, xlcShellDoCheck, 0);
    }
    if ( xlcShellTextEquals(cmd, "ir") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoIr);
    }
    if ( xlcShellTextEquals(cmd, "cgen") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoCgen);
    }
    if ( xlcShellTextEquals(cmd, "abi") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoAbi);
    }
    if ( xlcShellTextEquals(cmd, "run") ) {
        if ( xlcShellIsCSourcePath(xlcShellFileArg(argc, argv)) ) {
            return xlcShellRunC(argc, argv);
        }
        return xlcShellRunRunCommand(argc, argv);
    }
    if ( xlcShellTextEquals(cmd, "build") ) {
        if ( xlcShellIsCSourcePath(xlcShellFileArg(argc, argv)) ) {
            return xlcShellBuildC(argc, argv);
        }
        return xlcShellRunWithFile(argc, argv, xlcShellDoBuild);
    }
    xl6ConsolePrintf(XL6_CONSOLE_STDERR, "unknown command: %s\n", cmd);
    return 1;
}

static int xlcShellRetireModules(int code)
{
    XL_RuntimeDrainResult drained=xlRuntimeDrainRetiringChecked(-1);
    if(!drained.ready || !drained.ok){
        xl6ConsolePrintf(XL6_CONSOLE_STDERR,"runtime retirement incomplete or failed\n");
        if(!code)code=1;
    }
    return code;
}
#ifdef _WIN32
static char** xlcShellMakeUtf8Argv(int* outArgc)
{
    int argcW;
    int i;
    LPWSTR* argvW;
    char** argv;

    if ( outArgc == NULL ) {
        return NULL;
    }
    *outArgc = 0;
    argvW = CommandLineToArgvW(GetCommandLineW(), &argcW);
    if ( argvW == NULL || argcW <= 0 ) {
        return NULL;
    }
    argv = (char**)calloc((size_t)argcW + 1, sizeof(char*));
    if ( argv == NULL ) {
        LocalFree(argvW);
        return NULL;
    }
    for ( i = 0; i < argcW; ++i ) {
        int need = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
        if ( need <= 0 ) {
            continue;
        }
        argv[i] = (char*)malloc((size_t)need);
        if ( argv[i] != NULL ) {
            WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, argv[i], need, NULL, NULL);
        }
    }
    LocalFree(argvW);
    *outArgc = argcW;
    return argv;
}

static void xlcShellFreeUtf8Argv(int argc, char** argv)
{
    int i;

    if ( argv == NULL ) {
        return;
    }
    for ( i = 0; i < argc; ++i ) {
        free(argv[i]);
    }
    free(argv);
}

int main(void)
{
    int argc;
    char** argv;
    int rc;

    xl6ConsoleSessionBegin();
    argv = xlcShellMakeUtf8Argv(&argc);
    if ( argv == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot decode command line\n");
        xl6ConsoleSessionEnd();
        return 1;
    }
    rc = xlcShellRetireModules(xlcShellMain(argc, argv));
    xlcShellFreeUtf8Argv(argc, argv);
    xl6ConsoleSessionEnd();
    return rc;
}
#else
int main(int argc, char** argv)
{
    int rc;

    xl6ConsoleSessionBegin();
    rc = xlcShellRetireModules(xlcShellMain(argc, argv));
    xl6ConsoleSessionEnd();
    return rc;
}
#endif
