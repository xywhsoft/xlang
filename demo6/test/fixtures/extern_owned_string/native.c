#include <__xl_runtime/xruntime_decl.h>
#include <string.h>
static char* last_text;
char* owned_package_text(void)
{
    last_text=(char*)xrtMalloc(32);
    if(last_text)memcpy(last_text,"package text",13);
    return last_text;
}
char* owned_package_optional(bool present)
{
    if(!present)return NULL;
    char* result=owned_package_text();
    if(!result)xrtClearError(); /* NULL is the declared normal result. */
    return result;
}
bool owned_package_original(const char* value){return value==last_text;}
