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
static bool destroy_unadopted_font(void* raw)
{xge_font font=raw;if(font->iRefCount>1){xrtSetErrorKind(XERR_STATE);return false;}xgeFontFree(font);xrtFree(font);return true;}
static bool destroy_unadopted_texture(void* raw)
{xge_texture texture=raw;if(texture->iRefCount>1){xrtSetErrorKind(XERR_STATE);return false;}xgeTextureFree(texture);xrtFree(texture);return true;}
static bool destroy_image(void* raw){xgeImageFree(raw);xrtFree(raw);return true;}
static bool destroy_material(void* raw){xgeMaterialFree(raw);xrtFree(raw);return true;}
const XL6_ForeignViewDescriptorV1 declarativeXgeFontPolicy={sizeof(declarativeXgeFontPolicy),XL6_FOREIGN_VIEW_V1_TAG,"xge.font",0,1,NULL,NULL,destroy_font,0,1,NULL,NULL,NULL,NULL,0,destroy_unadopted_font,NULL};
const XL6_ForeignViewDescriptorV1 declarativeXgeTexturePolicy={sizeof(declarativeXgeTexturePolicy),XL6_FOREIGN_VIEW_V1_TAG,"xge.texture",0,1,NULL,NULL,destroy_texture,0,1,NULL,NULL,NULL,NULL,0,destroy_unadopted_texture,NULL};
const XL6_ForeignViewDescriptorV1 declarativeXgeImagePolicy={sizeof(declarativeXgeImagePolicy),XL6_FOREIGN_VIEW_V1_TAG,"xge.image",0,1,NULL,NULL,destroy_image,0,1,NULL,NULL,NULL,NULL,0,destroy_image,NULL};
const XL6_ForeignViewDescriptorV1 declarativeXgeMaterialPolicy={sizeof(declarativeXgeMaterialPolicy),XL6_FOREIGN_VIEW_V1_TAG,"xge.material",0,1,NULL,NULL,destroy_material,0,1,NULL,NULL,NULL,NULL,0,destroy_material,NULL};
xbytesview declarative_xge_image_bytes(void)
{
    /* A complete CRC-checked 1x1 RGBA PNG generated from one red pixel. */
    static const unsigned char bytes[]={0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,0x00,0x00,0x00,0x0d,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xf0,0x1f,0x00,0x05,0x00,0x01,0xff,0x89,0x99,0x3d,0x1d,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
    return (xbytesview){bytes,sizeof(bytes)};
}
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
