#include "src/common/xl6_output.h"

#include <stdio.h>
#include <string.h>

#define XL6_OUTPUT_BUFFER_STEP (64u * 1024u)

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
