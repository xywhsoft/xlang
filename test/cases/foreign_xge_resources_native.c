/* Resource policy hooks and fixture data only. The language declarations call
 * native XGE constructors/adopters/releases directly; no forwarding methods. */
#include "src/common/xl6_xrt.h"
#include "foreign_xge_resources.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool destroy_font(void* raw){xgeFontFree(raw);return true;}
static bool destroy_texture(void* raw){xgeTextureFree(raw);return true;}
const XL6_ForeignViewDescriptorV1 declarativeXgeFontPolicy={sizeof(declarativeXgeFontPolicy),XL6_FOREIGN_VIEW_V1_TAG,"xge.font",0,1,NULL,NULL,destroy_font,0,1,NULL,NULL,NULL,NULL};
const XL6_ForeignViewDescriptorV1 declarativeXgeTexturePolicy={sizeof(declarativeXgeTexturePolicy),XL6_FOREIGN_VIEW_V1_TAG,"xge.texture",0,1,NULL,NULL,destroy_texture,0,1,NULL,NULL,NULL,NULL};
xstrview declarative_xge_font_path(void)
{const char* path=getenv("XL6_TEST_FONT");assert(path);return (xstrview){path,strlen(path)};}
/* Fixture I/O is deliberately outside the binding under test. */
xbytesview declarative_xge_font_bytes(void)
{
    static unsigned char data[32768];static size_t size;
    if(!size){const char* path=getenv("XL6_TEST_FONT");assert(path);FILE* file=fopen(path,"rb");assert(file);size=fread(data,1,sizeof(data),file);assert(size>0 && size<sizeof(data) && !ferror(file));assert(fclose(file)==0);}
    return (xbytesview){data,size};
}
static xmemdebugsnapshot before;
void declarative_xge_heap_begin(void){xrtMemDebugSnapshot(&before);}
void declarative_xge_heap_end(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);}
void declarative_xge_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
void declarative_xge_reset_failure(void){assert(xrtMemDebugFailAfter(-1));}
