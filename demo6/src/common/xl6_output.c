#include "src/common/xl6_output.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define XL6_OUTPUT_BUFFER_STEP (64u * 1024u)
#define XL6_C_SYMBOL_PREFIX_MAX 28u

static unsigned int xl6OutputNextUtf8Codepoint(const unsigned char** cursor)
{
    const unsigned char* text;
    unsigned int codepoint;

    if ( cursor == NULL || *cursor == NULL || **cursor == '\0' ) return 0;
    text = *cursor;
    if ( text[0] < 0x80 ) {
        *cursor = text + 1;
        return text[0];
    }
    if ( (text[0] & 0xE0) == 0xC0 && text[1] != '\0' &&
         (text[1] & 0xC0) == 0x80 ) {
        codepoint = ((unsigned int)(text[0] & 0x1F) << 6) |
                    (unsigned int)(text[1] & 0x3F);
        if ( codepoint >= 0x80 ) {
            *cursor = text + 2;
            return codepoint;
        }
    } else if ( (text[0] & 0xF0) == 0xE0 && text[1] != '\0' &&
                text[2] != '\0' && (text[1] & 0xC0) == 0x80 &&
                (text[2] & 0xC0) == 0x80 ) {
        codepoint = ((unsigned int)(text[0] & 0x0F) << 12) |
                    ((unsigned int)(text[1] & 0x3F) << 6) |
                    (unsigned int)(text[2] & 0x3F);
        if ( codepoint >= 0x800 &&
             !(codepoint >= 0xD800 && codepoint <= 0xDFFF) ) {
            *cursor = text + 3;
            return codepoint;
        }
    } else if ( (text[0] & 0xF8) == 0xF0 && text[1] != '\0' &&
                text[2] != '\0' && text[3] != '\0' &&
                (text[1] & 0xC0) == 0x80 &&
                (text[2] & 0xC0) == 0x80 &&
                (text[3] & 0xC0) == 0x80 ) {
        codepoint = ((unsigned int)(text[0] & 0x07) << 18) |
                    ((unsigned int)(text[1] & 0x3F) << 12) |
                    ((unsigned int)(text[2] & 0x3F) << 6) |
                    (unsigned int)(text[3] & 0x3F);
        if ( codepoint >= 0x10000 && codepoint <= 0x10FFFF ) {
            *cursor = text + 4;
            return codepoint;
        }
    }

    *cursor = text + 1;
    return text[0];
}

static unsigned long long xl6OutputSymbolHash(const char* text)
{
    const unsigned char* cursor =
        (const unsigned char*)(text != NULL ? text : "");
    unsigned long long hash = 1469598103934665603ULL;
    while ( *cursor != '\0' ) {
        hash ^= (unsigned long long)*cursor++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static int xl6OutputIsCReservedIdentifier(const char* text)
{
    static const char* const names[] = {
        "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex",
        "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
        "_Thread_local", "auto", "break", "case", "char", "const",
        "continue", "default", "do", "double", "else", "enum",
        "extern", "float", "for", "goto", "if", "inline", "int",
        "long", "register", "restrict", "return", "short", "signed",
        "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while", "asm", "__asm",
        "__asm__", "__const", "__const__", "__volatile",
        "__volatile__", "__signed", "__signed__", "__inline",
        "__inline__", "__restrict", "__restrict__", "__extension__",
        "__thread", "__attribute", "__attribute__", "__alignof",
        "__alignof__", "typeof", "__typeof", "__typeof__", "__label__",
        "cdecl", "__cdecl", "__cdecl__", "stdcall", "__stdcall",
        "__stdcall__", "fastcall", "__fastcall", "__fastcall__",
        "thiscall", "__thiscall", "__thiscall__", "regparm",
        "__regparm__", "pascal", "near", "far", "huge", "interrupt",
        "based", "segment", "declspec", "__declspec", "errno", "stdin",
        "stdout", "stderr"
    };
    size_t i;
    if ( text == NULL ) return 0;
    for ( i = 0; i < sizeof(names) / sizeof(names[0]); ++i )
        if ( strcmp(text, names[i]) == 0 ) return 1;
    return 0;
}

static int xl6OutputLooksLikeCMacroIdentifier(const char* text)
{
    const unsigned char* cursor;
    int hasUpper = 0;
    if ( text == NULL || text[0] == '\0' ) return 0;
    for ( cursor = (const unsigned char*)text; *cursor != '\0'; ++cursor ) {
        if ( *cursor >= 'A' && *cursor <= 'Z' ) {
            hasUpper = 1;
        } else if ( (*cursor >= 'a' && *cursor <= 'z') ||
                    ((*cursor < '0' || *cursor > '9') && *cursor != '_') ) {
            return 0;
        }
    }
    return hasUpper;
}

int xl6OutputIsSimpleCIdentifier(const char* text)
{
    const unsigned char* cursor;
    if ( text == NULL || text[0] == '\0' ) return 0;
    cursor = (const unsigned char*)text;
    if ( !((*cursor >= 'a' && *cursor <= 'z') ||
           (*cursor >= 'A' && *cursor <= 'Z') || *cursor == '_') ) return 0;
    for ( ++cursor; *cursor != '\0'; ++cursor )
        if ( !((*cursor >= 'a' && *cursor <= 'z') ||
               (*cursor >= 'A' && *cursor <= 'Z') ||
               (*cursor >= '0' && *cursor <= '9') || *cursor == '_') )
            return 0;
    return !xl6OutputIsCReservedIdentifier(text);
}

static int xl6OutputEncodedCodepointText(
    char text[32], unsigned int codepoint)
{
    int written;
    if ( (codepoint >= 'a' && codepoint <= 'z') ||
         (codepoint >= 'A' && codepoint <= 'Z') ||
         (codepoint >= '0' && codepoint <= '9') ) {
        text[0] = (char)codepoint;
        text[1] = '\0';
        return 1;
    }
    if ( codepoint == '_' ) {
        text[0] = '_';
        text[1] = '_';
        text[2] = '\0';
        return 1;
    }
    written = snprintf(text, 32,
        codepoint <= 0xFFFFU ? "_u%04X_" : "_u%06X_", codepoint);
    return written >= 0 && written < 32;
}

int xl6OutputEncodeSymbolComponent(
    char* out, size_t capacity, const char* text)
{
    const unsigned char* cursor;
    size_t encodedSize = 0;
    size_t prefixSize = 0;
    char token[32];
    char prefix[XL6_C_SYMBOL_PREFIX_MAX + 1];
    int written;

    if ( out == NULL || capacity == 0 ) return 0;
    out[0] = '\0';
    if ( text == NULL || text[0] == '\0' ) {
        if ( capacity < 2 ) return 0;
        out[0] = '_';
        out[1] = '\0';
        return 1;
    }

    prefix[0] = '\0';
    cursor = (const unsigned char*)text;
    while ( *cursor != '\0' ) {
        size_t tokenSize;
        if ( !xl6OutputEncodedCodepointText(
                 token, xl6OutputNextUtf8Codepoint(&cursor)) ) return 0;
        tokenSize = strlen(token);
        if ( encodedSize > SIZE_MAX - tokenSize ) return 0;
        encodedSize += tokenSize;
        if ( prefixSize + tokenSize <= XL6_C_SYMBOL_PREFIX_MAX ) {
            memcpy(prefix + prefixSize, token, tokenSize + 1);
            prefixSize += tokenSize;
        }
    }

    if ( encodedSize <= XL6_C_SYMBOL_COMPONENT_MAX ) {
        size_t used = 0;
        if ( encodedSize + 1 > capacity ) return 0;
        cursor = (const unsigned char*)text;
        while ( *cursor != '\0' ) {
            size_t tokenSize;
            if ( !xl6OutputEncodedCodepointText(
                     token, xl6OutputNextUtf8Codepoint(&cursor)) ) return 0;
            tokenSize = strlen(token);
            memcpy(out + used, token, tokenSize);
            used += tokenSize;
        }
        out[used] = '\0';
        return 1;
    }

    written = snprintf(out, capacity, "%s_h%016llX", prefix,
        (unsigned long long)xl6OutputSymbolHash(text));
    if ( written < 0 || (size_t)written >= capacity ) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

static int xl6OutputCIdentifierCodepointText(
    char text[32], unsigned int codepoint, int first)
{
    int written;
    if ( (codepoint >= 'a' && codepoint <= 'z') ||
         (codepoint >= 'A' && codepoint <= 'Z') || codepoint == '_' ||
         (!first && codepoint >= '0' && codepoint <= '9') ) {
        text[0] = (char)codepoint;
        text[1] = '\0';
        return 1;
    }
    written = snprintf(text, 32,
        codepoint <= 0xFFFFU ? "_u%04X_" : "_u%06X_", codepoint);
    return written >= 0 && written < 32;
}

int xl6OutputEncodeCIdentifier(
    char* out, size_t capacity, const char* text)
{
    const unsigned char* cursor;
    size_t encodedSize;
    size_t prefixSize;
    int first;
    int reserved;
    char token[32];
    char prefix[XL6_C_SYMBOL_PREFIX_MAX + 1];
    int written;

    if ( out == NULL || capacity == 0 ) return 0;
    out[0] = '\0';
    if ( text == NULL || text[0] == '\0' ) {
        if ( capacity < 2 ) return 0;
        out[0] = '_';
        out[1] = '\0';
        return 1;
    }

    reserved = xl6OutputIsCReservedIdentifier(text) ||
               xl6OutputLooksLikeCMacroIdentifier(text);
    encodedSize = reserved ? sizeof("xl_kw_") - 1 : 0;
    prefixSize = 0;
    prefix[0] = '\0';
    if ( reserved ) {
        memcpy(prefix, "xl_kw_", sizeof("xl_kw_"));
        prefixSize = sizeof("xl_kw_") - 1;
    }
    cursor = (const unsigned char*)text;
    first = 1;
    while ( *cursor != '\0' ) {
        size_t tokenSize;
        if ( !xl6OutputCIdentifierCodepointText(
                 token, xl6OutputNextUtf8Codepoint(&cursor), first) ) return 0;
        tokenSize = strlen(token);
        if ( encodedSize > SIZE_MAX - tokenSize ) return 0;
        encodedSize += tokenSize;
        if ( prefixSize + tokenSize <= XL6_C_SYMBOL_PREFIX_MAX ) {
            memcpy(prefix + prefixSize, token, tokenSize + 1);
            prefixSize += tokenSize;
        }
        first = 0;
    }

    if ( encodedSize <= XL6_C_SYMBOL_COMPONENT_MAX ) {
        size_t used = 0;
        if ( encodedSize + 1 > capacity ) return 0;
        if ( reserved ) {
            memcpy(out, "xl_kw_", sizeof("xl_kw_") - 1);
            used = sizeof("xl_kw_") - 1;
        }
        cursor = (const unsigned char*)text;
        first = 1;
        while ( *cursor != '\0' ) {
            size_t tokenSize;
            if ( !xl6OutputCIdentifierCodepointText(
                     token, xl6OutputNextUtf8Codepoint(&cursor), first) ) return 0;
            tokenSize = strlen(token);
            memcpy(out + used, token, tokenSize);
            used += tokenSize;
            first = 0;
        }
        out[used] = '\0';
        return 1;
    }

    written = snprintf(out, capacity, "%s_h%016llX", prefix,
        (unsigned long long)xl6OutputSymbolHash(text));
    if ( written < 0 || (size_t)written >= capacity ) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

static void xl6OutputMarkFailed(XL6_Output* out)
{
    if ( out != NULL ) {
        if ( out->error[0] == '\0' ) {
            snprintf(out->error, sizeof(out->error), "%s", "output failed");
        }
        out->failed = 1;
    }
}

static void xl6OutputMarkFailedText(XL6_Output* out, const char* error)
{
    if ( out != NULL ) {
        if ( out->error[0] == '\0' ) {
            snprintf(
                out->error,
                sizeof(out->error),
                "%s",
                error != NULL && error[0] != '\0' ? error : "output failed"
            );
        }
        out->failed = 1;
    }
}

static void xl6OutputMarkFailedXrt(XL6_Output* out, const char* fallback)
{
    const xerror* lastError;
    const char* error;

    lastError = xrtGetError();
    error = lastError != NULL ? xrtErrorMessage(lastError) : NULL;
    if ( error == NULL || error[0] == '\0' ) {
        error = fallback;
    }
    xl6OutputMarkFailedText(out, error);
}

static int xl6OutputFlushBufferFile(XL6_Output* out)
{
    if ( out == NULL || out->path == NULL ) {
        return 0;
    }

    if ( !xrtFileWriteAtomic(out->path, xrtBufferView(&out->buffer)) ) {
        xl6OutputMarkFailedXrt(out, "cannot atomically write output file");
        return 0;
    }
    return 1;
}

static int xl6OutputFlushBufferStdFile(XL6_Output* out)
{
    size_t written;

    if ( out == NULL || out->stdFile == NULL ) {
        return 0;
    }

    if ( out->buffer.Size > 0 ) {
        written = fwrite(out->buffer.Data, 1, out->buffer.Size, out->stdFile);
        if ( written != out->buffer.Size ) {
            xl6OutputMarkFailedText(out, "cannot write output stream");
            return 0;
        }
    }

    if ( fflush(out->stdFile) != 0 ) {
        xl6OutputMarkFailedText(out, "cannot flush output stream");
        return 0;
    }
    return 1;
}

void xl6OutputInitStdFile(XL6_Output* out, FILE* file)
{
    if ( out == NULL ) {
        return;
    }
    out->kind = XL6_OUTPUT_STD_FILE;
    out->stdFile = file;
    out->path = NULL;
    memset(&out->buffer, 0, sizeof(out->buffer));
    out->error[0] = '\0';
    out->bufferActive = 0;
    out->failed = 0;
    if ( file == NULL ) {
        xl6OutputMarkFailedText(out, "invalid output stream");
    }
}

int xl6OutputOpenStdFileBuffer(XL6_Output* out, FILE* file)
{
    if ( out == NULL ) {
        return 0;
    }

    out->kind = XL6_OUTPUT_BUFFER_STD_FILE;
    out->stdFile = file;
    out->path = NULL;
    memset(&out->buffer, 0, sizeof(out->buffer));
    out->error[0] = '\0';
    out->bufferActive = 0;
    out->failed = 0;

    if ( file == NULL ) {
        xl6OutputMarkFailedText(out, "invalid output stream");
        return 0;
    }

    if ( !xrtBufferInit(&out->buffer) ) {
        xl6OutputMarkFailedXrt(out, "cannot create output buffer");
        return 0;
    }
    out->bufferActive = 1;
    if ( !xrtBufferReserve(&out->buffer, XL6_OUTPUT_BUFFER_STEP) ) {
        xl6OutputMarkFailedXrt(out, "cannot reserve output buffer");
        xrtBufferUnit(&out->buffer);
        out->bufferActive = 0;
        return 0;
    }
    return 1;
}

int xl6OutputOpenFile(XL6_Output* out, const char* path)
{
    if ( out == NULL ) {
        return 0;
    }
    out->kind = XL6_OUTPUT_BUFFER_FILE;
    out->stdFile = NULL;
    out->path = NULL;
    memset(&out->buffer, 0, sizeof(out->buffer));
    out->error[0] = '\0';
    out->bufferActive = 0;
    out->failed = 0;

    if ( !xrtBufferInit(&out->buffer) ) {
        xl6OutputMarkFailedXrt(out, "cannot create output buffer");
        return 0;
    }
    out->bufferActive = 1;
    if ( !xrtBufferReserve(&out->buffer, XL6_OUTPUT_BUFFER_STEP) ) {
        xl6OutputMarkFailedXrt(out, "cannot reserve output buffer");
        xrtBufferUnit(&out->buffer);
        out->bufferActive = 0;
        return 0;
    }

    if ( path == NULL ) {
        xl6OutputMarkFailedText(out, "output path is empty");
        xrtBufferUnit(&out->buffer);
        out->bufferActive = 0;
        return 0;
    }

    out->path = xrtStrDupN(path, strlen(path));
    if ( out->path == NULL ) {
        xl6OutputMarkFailedXrt(out, "cannot copy output path");
        xrtBufferUnit(&out->buffer);
        out->bufferActive = 0;
        return 0;
    }
    return 1;
}

int xl6OutputOpenMemory(XL6_Output* out)
{
    if ( out == NULL ) {
        return 0;
    }
    out->kind = XL6_OUTPUT_BUFFER_MEMORY;
    out->stdFile = NULL;
    out->path = NULL;
    memset(&out->buffer, 0, sizeof(out->buffer));
    out->error[0] = '\0';
    out->bufferActive = 0;
    out->failed = 0;

    if ( !xrtBufferInit(&out->buffer) ) {
        xl6OutputMarkFailedXrt(out, "cannot create output buffer");
        return 0;
    }
    out->bufferActive = 1;
    if ( !xrtBufferReserve(&out->buffer, XL6_OUTPUT_BUFFER_STEP) ) {
        xl6OutputMarkFailedXrt(out, "cannot reserve output buffer");
        xrtBufferUnit(&out->buffer);
        out->bufferActive = 0;
        return 0;
    }
    return 1;
}

char* xl6OutputTakeMemoryText(XL6_Output* out, size_t* outSize)
{
    char* text;
    size_t len;

    if ( outSize != NULL ) {
        *outSize = 0;
    }
    if ( out == NULL || out->kind != XL6_OUTPUT_BUFFER_MEMORY || out->failed || !out->bufferActive ) {
        return NULL;
    }
    len = out->buffer.Size;
    text = (char*)xrtMalloc(len + 1);
    if ( text == NULL ) {
        xl6OutputMarkFailedXrt(out, "cannot copy output buffer");
        return NULL;
    }
    if ( len > 0 ) {
        memcpy(text, out->buffer.Data, len);
    }
    text[len] = '\0';
    if ( outSize != NULL ) {
        *outSize = len;
    }
    return text;
}

void xl6OutputDiscard(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }
    if ( out->kind == XL6_OUTPUT_BUFFER_FILE ) {
        xrtFree(out->path);
        out->path = NULL;
    }
    if ( out->bufferActive ) {
        xrtBufferUnit(&out->buffer);
        out->bufferActive = 0;
    }
    out->stdFile = NULL;
}

void xl6OutputClose(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }
    if ( out->kind == XL6_OUTPUT_BUFFER_FILE ) {
        if ( !out->failed && !xl6OutputFlushBufferFile(out) ) {
            out->failed = 1;
        }
        xrtFree(out->path);
        out->path = NULL;
        xrtBufferUnit(&out->buffer);
        out->bufferActive = 0;
    } else if ( out->kind == XL6_OUTPUT_BUFFER_STD_FILE ) {
        if ( !out->failed && !xl6OutputFlushBufferStdFile(out) ) {
            out->failed = 1;
        }
        xrtBufferUnit(&out->buffer);
        out->bufferActive = 0;
    } else if ( out->kind == XL6_OUTPUT_BUFFER_MEMORY ) {
        xrtBufferUnit(&out->buffer);
        out->bufferActive = 0;
    } else if ( out->kind == XL6_OUTPUT_STD_FILE && out->stdFile != NULL ) {
        fflush(out->stdFile);
    }
    out->stdFile = NULL;
}

int xl6OutputHasFailed(const XL6_Output* out)
{
    return out == NULL || out->failed;
}

const char* xl6OutputError(const XL6_Output* out)
{
    if ( out == NULL || out->error[0] == '\0' ) {
        return "output failed";
    }
    return out->error;
}

int xl6OutputPuts(const char* text, XL6_Output* out)
{
    xbytesview data;
    size_t len;

    if ( out == NULL || out->failed ) {
        return EOF;
    }

    text = text != NULL ? text : "";
    len = strlen(text);
    if ( len == 0 ) {
        return 0;
    }

    if ( out->kind == XL6_OUTPUT_BUFFER_FILE || out->kind == XL6_OUTPUT_BUFFER_STD_FILE || out->kind == XL6_OUTPUT_BUFFER_MEMORY ) {
        data.Data = (const unsigned char*)text;
        data.Size = len;
        if ( !out->bufferActive || !xrtBufferAppend(&out->buffer, data) ) {
            xl6OutputMarkFailedXrt(out, "cannot append output buffer");
            return EOF;
        }
        return 0;
    }

    if ( out->stdFile == NULL || fwrite(text, 1, len, out->stdFile) != len ) {
        xl6OutputMarkFailed(out);
        return EOF;
    }
    return 0;
}

int xl6OutputPutc(int ch, XL6_Output* out)
{
    unsigned char byte;

    if ( out == NULL || out->failed ) {
        return EOF;
    }

    if ( out->kind == XL6_OUTPUT_BUFFER_FILE || out->kind == XL6_OUTPUT_BUFFER_STD_FILE || out->kind == XL6_OUTPUT_BUFFER_MEMORY ) {
        if ( !out->bufferActive || !xrtBufferAppendByte(&out->buffer, (uint8)ch) ) {
            xl6OutputMarkFailedXrt(out, "cannot append output buffer");
            return EOF;
        }
        return ch;
    }

    byte = (unsigned char)ch;
    if ( out->stdFile == NULL || fwrite(&byte, 1, 1, out->stdFile) != 1 ) {
        xl6OutputMarkFailed(out);
        return EOF;
    }
    return ch;
}

int xl6OutputVPrintf(XL6_Output* out, const char* format, va_list args)
{
    char stackBuf[1024];
    char* heapBuf;
    va_list argsCopy;
    int needed;
    int rc;

    if ( out == NULL || out->failed || format == NULL ) {
        return EOF;
    }

    va_copy(argsCopy, args);
    needed = vsnprintf(stackBuf, sizeof(stackBuf), format, argsCopy);
    va_end(argsCopy);
    if ( needed < 0 ) {
        xl6OutputMarkFailedText(out, "cannot format output text");
        return EOF;
    }

    if ( (size_t)needed < sizeof(stackBuf) ) {
        rc = xl6OutputPuts(stackBuf, out);
        return rc == EOF ? EOF : needed;
    }

    heapBuf = (char*)xrtMalloc((size_t)needed + 1u);
    if ( heapBuf == NULL ) {
        xl6OutputMarkFailedXrt(out, "cannot allocate output buffer");
        return EOF;
    }

    va_copy(argsCopy, args);
    rc = vsnprintf(heapBuf, (size_t)needed + 1u, format, argsCopy);
    va_end(argsCopy);
    if ( rc < 0 ) {
        xrtFree(heapBuf);
        xl6OutputMarkFailedText(out, "cannot format output text");
        return EOF;
    }

    rc = xl6OutputPuts(heapBuf, out);
    xrtFree(heapBuf);
    return rc == EOF ? EOF : needed;
}

int xl6OutputPrintf(XL6_Output* out, const char* format, ...)
{
    va_list args;
    int rc;

    va_start(args, format);
    rc = xl6OutputVPrintf(out, format, args);
    va_end(args);
    return rc;
}
