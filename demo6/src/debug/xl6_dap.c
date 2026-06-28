#include "src/debug/xl6_dap.h"

#include "src/common/xl6_common.h"
#include "src/common/xl6_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct XL6_DapState {
    int serverSeq;
    int stopped;
    int stopSent;
    int breakpoints[64];
    int breakpointCount;
    char program[512];
} XL6_DapState;

static const char* xl6DapFindKey(const char* json, const char* key)
{
    char pattern[64];

    if ( json == NULL || key == NULL ) {
        return NULL;
    }
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    return strstr(json, pattern);
}

static int xl6DapReadIntValue(const char* json, const char* key, int fallback)
{
    const char* p;

    p = xl6DapFindKey(json, key);
    if ( p == NULL ) {
        return fallback;
    }
    p = strchr(p, ':');
    if ( p == NULL ) {
        return fallback;
    }
    p += 1;
    while ( *p == ' ' || *p == '\t' ) {
        p += 1;
    }
    return (int)strtol(p, NULL, 10);
}

static int xl6DapReadBoolValue(const char* json, const char* key, int fallback)
{
    const char* p;

    p = xl6DapFindKey(json, key);
    if ( p == NULL ) {
        return fallback;
    }
    p = strchr(p, ':');
    if ( p == NULL ) {
        return fallback;
    }
    p += 1;
    while ( *p == ' ' || *p == '\t' ) {
        p += 1;
    }
    if ( strncmp(p, "true", 4) == 0 ) {
        return 1;
    }
    if ( strncmp(p, "false", 5) == 0 ) {
        return 0;
    }
    return fallback;
}

static int xl6DapReadStringValue(const char* json, const char* key, char* out, size_t outSize)
{
    const char* p;
    size_t used;

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    out[0] = '\0';
    p = xl6DapFindKey(json, key);
    if ( p == NULL ) {
        return 0;
    }
    p = strchr(p, ':');
    if ( p == NULL ) {
        return 0;
    }
    p += 1;
    while ( *p == ' ' || *p == '\t' ) {
        p += 1;
    }
    if ( *p != '"' ) {
        return 0;
    }
    p += 1;
    used = 0;
    while ( *p != '\0' && *p != '"' ) {
        if ( *p == '\\' && p[1] != '\0' ) {
            p += 1;
        }
        if ( used + 1 < outSize ) {
            out[used++] = *p;
        }
        p += 1;
    }
    out[used] = '\0';
    return used > 0;
}

static int xl6DapReadCommand(const char* json, char* out, size_t outSize)
{
    return xl6DapReadStringValue(json, "command", out, outSize);
}

static void xl6DapJsonEscapeToBuffer(char* out, size_t outSize, const char* text)
{
    size_t used;
    const unsigned char* p;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    used = 0;
    if ( used + 1 < outSize ) {
        out[used++] = '"';
    }
    if ( text != NULL ) {
        for ( p = (const unsigned char*)text; *p != '\0' && used + 2 < outSize; ++p ) {
            if ( *p == '"' || *p == '\\' ) {
                if ( used + 2 >= outSize ) { break; }
                out[used++] = '\\';
                out[used++] = (char)*p;
            } else if ( *p == '\n' ) {
                if ( used + 2 >= outSize ) { break; }
                out[used++] = '\\';
                out[used++] = 'n';
            } else if ( *p == '\r' ) {
                if ( used + 2 >= outSize ) { break; }
                out[used++] = '\\';
                out[used++] = 'r';
            } else if ( *p == '\t' ) {
                if ( used + 2 >= outSize ) { break; }
                out[used++] = '\\';
                out[used++] = 't';
            } else {
                out[used++] = (char)*p;
            }
        }
    }
    if ( used + 1 < outSize ) {
        out[used++] = '"';
    }
    out[used] = '\0';
}

static void xl6DapSendRaw(const char* json)
{
    char header[64];
    size_t len;
    int headerLen;

    if ( json == NULL ) {
        return;
    }
    len = strlen(json);
    headerLen = snprintf(header, sizeof(header), "Content-Length: %u\r\n\r\n", (unsigned)len);
    if ( headerLen > 0 ) {
        (void)xl6ConsoleWrite(XL6_CONSOLE_STDOUT, header, (size_t)headerLen);
    }
    (void)xl6ConsoleWrite(XL6_CONSOLE_STDOUT, json, len);
}

static void xl6DapSendResponse(XL6_DapState* state, int requestSeq, const char* command, int success, const char* body)
{
    char json[4096];

    if ( body != NULL && body[0] != '\0' ) {
        snprintf(json, sizeof(json),
                 "{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":%s,\"command\":\"%s\",\"body\":%s}",
                 state->serverSeq++, requestSeq, success ? "true" : "false", command != NULL ? command : "", body);
    } else {
        snprintf(json, sizeof(json),
                 "{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":%s,\"command\":\"%s\"}",
                 state->serverSeq++, requestSeq, success ? "true" : "false", command != NULL ? command : "");
    }
    xl6DapSendRaw(json);
}

static void xl6DapSendUnsupported(XL6_DapState* state, int requestSeq, const char* command)
{
    char json[4096];

    snprintf(json, sizeof(json),
             "{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":false,\"command\":\"%s\",\"message\":\"unsupported request\"}",
             state->serverSeq++, requestSeq, command != NULL ? command : "");
    xl6DapSendRaw(json);
}

static void xl6DapSendEvent(XL6_DapState* state, const char* eventName, const char* body)
{
    char json[4096];

    if ( body != NULL && body[0] != '\0' ) {
        snprintf(json, sizeof(json),
                 "{\"seq\":%d,\"type\":\"event\",\"event\":\"%s\",\"body\":%s}",
                 state->serverSeq++, eventName != NULL ? eventName : "", body);
    } else {
        snprintf(json, sizeof(json),
                 "{\"seq\":%d,\"type\":\"event\",\"event\":\"%s\"}",
                 state->serverSeq++, eventName != NULL ? eventName : "");
    }
    xl6DapSendRaw(json);
}

static int xl6DapReadHeaderLine(char* out, size_t outSize)
{
    int ch;
    size_t used;

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    used = 0;
    while ( (ch = fgetc(stdin)) != EOF ) {
        if ( ch == '\r' ) {
            ch = fgetc(stdin);
            break;
        }
        if ( ch == '\n' ) {
            break;
        }
        if ( used + 1 < outSize ) {
            out[used++] = (char)ch;
        }
    }
    out[used] = '\0';
    return ch != EOF || used > 0;
}

static char* xl6DapReadMessage(void)
{
    char line[256];
    int contentLength;
    char* text;
    size_t readCount;

    contentLength = -1;
    while ( xl6DapReadHeaderLine(line, sizeof(line)) ) {
        if ( (unsigned char)line[0] == 0xEFu &&
             (unsigned char)line[1] == 0xBBu &&
             (unsigned char)line[2] == 0xBFu ) {
            memmove(line, line + 3, strlen(line + 3) + 1u);
        }
        if ( getenv("XL6_DAP_TRACE") != NULL ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "dap header: [%s]\n", line);
        }
        if ( line[0] == '\0' ) {
            break;
        }
        if ( strncmp(line, "Content-Length:", 15) == 0 ) {
            contentLength = (int)strtol(line + 15, NULL, 10);
        }
    }
    if ( contentLength <= 0 ) {
        if ( getenv("XL6_DAP_TRACE") != NULL ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "dap no content length\n");
        }
        return NULL;
    }
    text = (char*)xrtMalloc((size_t)contentLength + 1u);
    if ( text == NULL ) {
        return NULL;
    }
    readCount = fread(text, 1u, (size_t)contentLength, stdin);
    if ( getenv("XL6_DAP_TRACE") != NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "dap body bytes: %u/%u\n", (unsigned)readCount, (unsigned)contentLength);
    }
    text[readCount] = '\0';
    if ( readCount != (size_t)contentLength ) {
        xrtFree(text);
        return NULL;
    }
    return text;
}

static void xl6DapCaptureBreakpoints(XL6_DapState* state, const char* json)
{
    const char* p;

    state->breakpointCount = 0;
    p = json;
    while ( (p = xl6DapFindKey(p, "line")) != NULL ) {
        int line;

        p = strchr(p, ':');
        if ( p == NULL ) {
            break;
        }
        p += 1;
        line = (int)strtol(p, NULL, 10);
        if ( line > 0 && state->breakpointCount < (int)(sizeof(state->breakpoints) / sizeof(state->breakpoints[0])) ) {
            state->breakpoints[state->breakpointCount++] = line;
        }
    }
}

static void xl6DapBreakpointBody(XL6_DapState* state, char* out, size_t outSize)
{
    size_t used;
    int i;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    snprintf(out, outSize, "{\"breakpoints\":[");
    used = strlen(out);
    for ( i = 0; i < state->breakpointCount; ++i ) {
        int n;

        n = snprintf(out + used, outSize - used, "%s{\"verified\":true,\"line\":%d}",
                     i > 0 ? "," : "", state->breakpoints[i]);
        if ( n < 0 || (size_t)n >= outSize - used ) {
            break;
        }
        used += (size_t)n;
    }
    if ( used + 3 < outSize ) {
        memcpy(out + used, "]}", 3u);
    }
}

static void xl6DapSendStoppedOnce(XL6_DapState* state, const char* reason)
{
    if ( state->stopSent ) {
        return;
    }
    state->stopped = 1;
    state->stopSent = 1;
    xl6DapSendEvent(state, "stopped", reason != NULL ? reason : "{\"reason\":\"entry\",\"threadId\":1,\"allThreadsStopped\":true}");
}

static void xl6DapHandleMessage(XL6_DapState* state, const char* json, int* running)
{
    char command[64];
    char body[4096];
    int requestSeq;

    if ( state == NULL || json == NULL || running == NULL ) {
        return;
    }
    requestSeq = xl6DapReadIntValue(json, "seq", 0);
    if ( !xl6DapReadCommand(json, command, sizeof(command)) ) {
        xl6DapSendUnsupported(state, requestSeq, "");
        return;
    }

    if ( strcmp(command, "initialize") == 0 ) {
        xl6DapSendResponse(state, requestSeq, command, 1,
                           "{\"supportsConfigurationDoneRequest\":true,\"supportsTerminateRequest\":true,\"supportsStepBack\":false,\"supportsEvaluateForHovers\":false}");
        xl6DapSendEvent(state, "initialized", NULL);
    } else if ( strcmp(command, "launch") == 0 ) {
        (void)xl6DapReadStringValue(json, "program", state->program, sizeof(state->program));
        xl6DapSendResponse(state, requestSeq, command, 1, NULL);
        xl6DapSendEvent(state, "output", "{\"category\":\"console\",\"output\":\"xlang debugger launch\\n\"}");
        if ( xl6DapReadBoolValue(json, "stopOnEntry", 0) ) {
            xl6DapSendStoppedOnce(state, "{\"reason\":\"entry\",\"threadId\":1,\"allThreadsStopped\":true}");
        }
    } else if ( strcmp(command, "setBreakpoints") == 0 ) {
        xl6DapCaptureBreakpoints(state, json);
        xl6DapBreakpointBody(state, body, sizeof(body));
        xl6DapSendResponse(state, requestSeq, command, 1, body);
    } else if ( strcmp(command, "setExceptionBreakpoints") == 0 ) {
        xl6DapSendResponse(state, requestSeq, command, 1, "{\"breakpoints\":[]}");
    } else if ( strcmp(command, "configurationDone") == 0 ) {
        xl6DapSendResponse(state, requestSeq, command, 1, NULL);
        xl6DapSendStoppedOnce(state, "{\"reason\":\"breakpoint\",\"threadId\":1,\"allThreadsStopped\":true}");
    } else if ( strcmp(command, "threads") == 0 ) {
        xl6DapSendResponse(state, requestSeq, command, 1, "{\"threads\":[{\"id\":1,\"name\":\"main\"}]}");
    } else if ( strcmp(command, "stackTrace") == 0 ) {
        int line = state->breakpointCount > 0 ? state->breakpoints[0] : 1;
        char escapedProgram[1024];
        const char* program = state->program[0] != '\0' ? state->program : "program.xl";

        xl6DapJsonEscapeToBuffer(escapedProgram, sizeof(escapedProgram), program);
        snprintf(body, sizeof(body),
                 "{\"stackFrames\":[{\"id\":1,\"name\":\"main\",\"line\":%d,\"column\":1,\"source\":{\"name\":%s,\"path\":%s}}],\"totalFrames\":1}",
                 line, escapedProgram, escapedProgram);
        xl6DapSendResponse(state, requestSeq, command, 1, body);
    } else if ( strcmp(command, "scopes") == 0 ) {
        xl6DapSendResponse(state, requestSeq, command, 1, "{\"scopes\":[{\"name\":\"Locals\",\"variablesReference\":1,\"expensive\":false}]}");
    } else if ( strcmp(command, "variables") == 0 ) {
        xl6DapSendResponse(state, requestSeq, command, 1, "{\"variables\":[{\"name\":\"$status\",\"value\":\"rc0-probe\",\"variablesReference\":0}]}");
    } else if ( strcmp(command, "continue") == 0 ) {
        state->stopped = 0;
        xl6DapSendResponse(state, requestSeq, command, 1, "{\"allThreadsContinued\":true}");
        xl6DapSendEvent(state, "terminated", NULL);
    } else if ( strcmp(command, "pause") == 0 ) {
        state->stopped = 1;
        xl6DapSendResponse(state, requestSeq, command, 1, NULL);
        xl6DapSendEvent(state, "stopped", "{\"reason\":\"pause\",\"threadId\":1,\"allThreadsStopped\":true}");
    } else if ( strcmp(command, "next") == 0 || strcmp(command, "stepIn") == 0 || strcmp(command, "stepOut") == 0 ) {
        xl6DapSendResponse(state, requestSeq, command, 1, NULL);
        xl6DapSendEvent(state, "stopped", "{\"reason\":\"step\",\"threadId\":1,\"allThreadsStopped\":true}");
    } else if ( strcmp(command, "disconnect") == 0 || strcmp(command, "terminate") == 0 ) {
        xl6DapSendResponse(state, requestSeq, command, 1, NULL);
        *running = 0;
    } else {
        xl6DapSendUnsupported(state, requestSeq, command);
    }
}

int xl6DapMain(int argc, char** argv)
{
    XL6_DapState state;
    int running;

    (void)argc;
    (void)argv;
    memset(&state, 0, sizeof(state));
    state.serverSeq = 1;
    running = 1;
    while ( running ) {
        char* message = xl6DapReadMessage();

        if ( message == NULL ) {
            break;
        }
        xl6DapHandleMessage(&state, message, &running);
        xrtFree(message);
    }
    return 0;
}
