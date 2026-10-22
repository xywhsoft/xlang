#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static int64_t count;
void* c_owned_text(void)
{
    char* data=malloc(6); assert(data); memcpy(data,"hello",6); return data;
}
void* c_owned_binary(void)
{
    const unsigned char bytes[]={0,255,17,42};
    void* data=malloc(sizeof(bytes)); assert(data); memcpy(data,bytes,sizeof(bytes)); return data;
}
void c_owned_release(void* data) { assert(data); ++count; free(data); }
int64_t c_owned_released(void) { return count; }
