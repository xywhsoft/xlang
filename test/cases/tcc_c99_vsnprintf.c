#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int format(char* output, size_t capacity, const char* text, ...)
{
    va_list arguments;
    va_start(arguments, text);
    int result = vsnprintf(output, capacity, text, arguments);
    va_end(arguments);
    return result;
}

int main(void)
{
    char text[4097], exact[4099], shortText[19], one[1], untouched='!';
    memset(text, 'x', sizeof(text)-1); text[sizeof(text)-1]='\0';
    assert(format(NULL, 0, "%s:%d", text, 7) == 4098);
    assert(format(&untouched, 0, "%s:%d", text, 7) == 4098 && untouched=='!');
    assert(format(shortText, sizeof(shortText), "%s:%d", text, 7) == 4098);
    assert(shortText[sizeof(shortText)-1]=='\0');
    for (size_t i=0; i<sizeof(shortText)-1; ++i) assert(shortText[i]=='x');
    assert(format(one, sizeof(one), "ab") == 2 && one[0]=='\0');
    assert(format(one, sizeof(one), "a") == 1 && one[0]=='\0');
    assert(format(one, sizeof(one), "") == 0 && one[0]=='\0');
    assert(format(exact, sizeof(exact), "%s:%d", text, 7) == 4098);
    assert(!memcmp(exact, text, 4096) && !strcmp(exact+4096, ":7"));
    assert(format(shortText, sizeof(shortText), "%s:%d", "ok", 42) == 5);
    assert(!strcmp(shortText, "ok:42"));
    puts("C99 vsnprintf contracts ok");
    return 0;
}
