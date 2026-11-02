/* 与 mask.xl 相同：输入、字体、字形退役后只留下独立像素快照。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_FILE_WHOLE
#include "xrt.h"
#include "../include/stb_truetype_native.h"
#ifdef __TINYC__
#include "../stb_truetype_native.c"
#endif
#include <stdio.h>
static xbuffer* mask(const char* path,int* width,int* height)
{
    size_t size=0;bytes encoded=xrtFileReadAllLimit(path,32u*1024u*1024u,&size);
    void* font=encoded ? xstbtt_load_memory(encoded,size,0) : NULL;
    xrtFree(encoded);
    void* glyph=font ? xstbtt_font_bitmap(font,65,12.0) : NULL;
    xbuffer* pixels=NULL;
    if(glyph){
        *width=xstbtt_bitmap_width(glyph);*height=xstbtt_bitmap_height(glyph);
        pixels=xrtBufferCreate();
        if(pixels && !xrtBufferAppend(pixels,(xbytesview){xstbtt_bitmap_data(glyph),xstbtt_bitmap_size(glyph)})){
            xrtBufferDestroy(pixels);pixels=NULL;
        }
    }
    xstbtt_bitmap_release(glyph);xstbtt_font_release(font);return pixels;
}
int main(int argc,char** argv)
{
    const char* path=argc>1 ? argv[1] : "release/extlibs/stb_truetype/test/assets/DejaVuSans-test.ttf";
    int width=0,height=0;xbuffer* pixels=mask(path,&width,&height);
    if(!pixels)return 1;
    xbytesview view=xrtBufferView(pixels);
    if(view.Size!=(size_t)width*(size_t)height){xrtBufferDestroy(pixels);return 1;}
    puts("glyph A mask");
    for(int y=0;y<height;++y){
        for(int x=0;x<width;++x){unsigned char alpha=((const unsigned char*)view.Data)[y*width+x];putchar(alpha>=128 ? '#' : alpha>0 ? '+' : '.');}
        putchar('\n');
    }
    puts("independent snapshot: true");xrtBufferDestroy(pixels);return 0;
}
