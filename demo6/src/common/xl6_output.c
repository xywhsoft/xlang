#include "src/common/xl6_output.h"

#include <string.h>

#define XL6_OUTPUT_BUFFER_STEP (64u * 1024u)

static void xl6OutputMarkFailed(XL6_Output* out)
{
    if ( out != NULL ) {
        if ( out->error == NULL ) {
            out->error = "output failed";
        }
        out->failed = 1;
    }
}

static void xl6OutputMarkFailedText(XL6_Output* out, const char* error)
{
    if ( out != NULL ) {
        out->error = error != NULL && error[0] != '\0' ? error : "output failed";
        out->failed = 1;
    }
}

static void xl6OutputMarkFailedXrt(XL6_Output* out, const char* fallback)
{
    const char* error;

    error = (const char*)xrtGetError();
    if ( error == NULL || error[0] == '\0' ) {
        error = fallback;
    }
    xl6OutputMarkFailedText(out, error);
}

static int xl6OutputFlushBufferFile(XL6_Output* out)
{
    xfile file;
    size_t written;

    if ( out == NULL || out->path == NULL ) {
        return 0;
    }

    xrtClearError();
    file = xrtOpen((char*)out->path, FALSE, XRT_CP_BINARY);
    if ( file == NULL ) {
        xl6OutputMarkFailedXrt(out, "cannot open output file");
        return 0;
    }

    xrtSeek(file, 0, XRT_SEEK_SET);
    if ( out->buffer != NULL && out->buffer->Length > 0 ) {
        written = xrtPut(file, out->buffer->Buffer, out->buffer->Length);
        if ( written != out->buffer->Length ) {
            xl6OutputMarkFailedXrt(out, "cannot write output file");
            xrtClose(file);
            return 0;
        }
    }
    if ( !xrtSetEOF(file) ) {
        xl6OutputMarkFailedXrt(out, "cannot truncate output file");
        xrtClose(file);
        return 0;
    }
    xrtClose(file);
    return 1;
}

static int xl6OutputFlushBufferStdFile(XL6_Output* out)
{
    size_t written;

    if ( out == NULL || out->stdFile == NULL ) {
        return 0;
    }

    if ( out->buffer != NULL && out->buffer->Length > 0 ) {
        written = fwrite(out->buffer->Buffer, 1, out->buffer->Length, out->stdFile);
        if ( written != out->buffer->Length ) {
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
    out->buffer = NULL;
    out->error = file == NULL ? "invalid output stream" : NULL;
    out->failed = file == NULL;
}

int xl6OutputOpenStdFileBuffer(XL6_Output* out, FILE* file)
{
    if ( out == NULL ) {
        return 0;
    }

    out->kind = XL6_OUTPUT_BUFFER_STD_FILE;
    out->stdFile = file;
    out->path = NULL;
    out->buffer = NULL;
    out->error = NULL;
    out->failed = 0;

    if ( file == NULL ) {
        xl6OutputMarkFailedText(out, "invalid output stream");
        return 0;
    }

    out->buffer = xrtBufferCreate(XL6_OUTPUT_BUFFER_STEP);
    if ( out->buffer == NULL ) {
        xl6OutputMarkFailedXrt(out, "cannot create output buffer");
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
    out->buffer = xrtBufferCreate(XL6_OUTPUT_BUFFER_STEP);
    out->error = NULL;
    out->failed = 0;

    if ( out->buffer == NULL ) {
        xl6OutputMarkFailedXrt(out, "cannot create output buffer");
        return 0;
    }

    if ( path == NULL ) {
        xl6OutputMarkFailedText(out, "output path is empty");
        xrtBufferDestroy(out->buffer);
        out->buffer = NULL;
        return 0;
    }

    out->path = xrtCopyStr((char*)path, strlen(path));
    if ( out->path == NULL ) {
        xl6OutputMarkFailedXrt(out, "cannot copy output path");
        xrtBufferDestroy(out->buffer);
        out->buffer = NULL;
        return 0;
    }
    return 1;
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
        xrtBufferDestroy(out->buffer);
        out->buffer = NULL;
    } else if ( out->kind == XL6_OUTPUT_BUFFER_STD_FILE ) {
        if ( !out->failed && !xl6OutputFlushBufferStdFile(out) ) {
            out->failed = 1;
        }
        xrtBufferDestroy(out->buffer);
        out->buffer = NULL;
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
    if ( out == NULL || out->error == NULL || out->error[0] == '\0' ) {
        return "output failed";
    }
    return out->error;
}

int xl6OutputPuts(const char* text, XL6_Output* out)
{
    size_t len;

    if ( out == NULL || out->failed ) {
        return EOF;
    }

    text = text != NULL ? text : "";
    len = strlen(text);
    if ( len == 0 ) {
        return 0;
    }

    if ( out->kind == XL6_OUTPUT_BUFFER_FILE || out->kind == XL6_OUTPUT_BUFFER_STD_FILE ) {
        if ( out->buffer == NULL || len > 0xFFFFFFFFu || !xrtBufferAppend(out->buffer, (char*)text, (uint32)len, XBUF_BINARY) ) {
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
    char text[1];

    if ( out == NULL || out->failed ) {
        return EOF;
    }

    text[0] = (char)ch;
    if ( out->kind == XL6_OUTPUT_BUFFER_FILE || out->kind == XL6_OUTPUT_BUFFER_STD_FILE ) {
        if ( out->buffer == NULL || !xrtBufferAppend(out->buffer, text, 1, XBUF_BINARY) ) {
            xl6OutputMarkFailedXrt(out, "cannot append output buffer");
            return EOF;
        }
        return ch;
    }

    if ( out->stdFile == NULL || fwrite(text, 1, 1, out->stdFile) != 1 ) {
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
