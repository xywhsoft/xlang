#include "src/runtime/xl6_process_text.h"

/* Acceptance by the OS association/launcher is not application completion.
 * Selection precedes UTF-8/NUL validation; no shell interpolation is added. */
bool xl6ProcessOpenV1(xstrview target,int64_t size)
{
    char* text=xl6ProcessCString(target,size,true);
    if(!text)return false;
    bool accepted=xrtProcessOpen(text);
    xrtFree(text);
    return accepted;
}
