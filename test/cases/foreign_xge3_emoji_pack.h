#ifndef XL6_FIXTURE_XGE3_EMOJI_PACK_H
#define XL6_FIXTURE_XGE3_EMOJI_PACK_H
#include <xge3_runtime.h>

/* Leave one native publication reference. Language views may otherwise
 * still be waiting for checked retirement when the next callback runs. */
static int fixtureEmojiDrawPublishedRaster(xge_font font,const void* data,int size)
{
    const char key[]="\xf0\x9f\x98\x80";
    xge_emoji_pack pack=NULL;
    int status=xgeEmojiPackCreate(&pack);
    if(status==XGE_OK)status=xgeEmojiPackAddSvgMemory(pack,key,data,size,NULL,NULL);
    if(status==XGE_OK){
        xge_text_shape_desc_t desc={0};
        desc.iSize=sizeof(desc);desc.pFont=font;desc.sText=key;desc.iTextSize=4;
        desc.iFlags=XGE_TEXT_SHAPE_EMOJI;desc.pEmojiPack=pack;
        xge_glyph_run_t* run=NULL;
        status=xgeTextShapeOwned(&run,&desc);
        if(status==XGE_OK){
            xgeGlyphRunDraw(run,5,5,0xffffffffu,XGE_DRAW_SCREEN_SPACE);
            xgeGlyphRunFree(run);
            status=xgeEmojiPackSetDefault(pack);
        }
    }
    xgeEmojiPackFree(pack);
    return status;
}

static int fixtureEmojiPackReferences(void)
{
    xge_emoji_pack pack=NULL;
    int status=xgeEmojiPackGetDefault(&pack);
    if(status!=XGE_OK)return status;
    int retained=xgeEmojiPackAddRef(pack);
    xgeEmojiPackFree(pack);
    if(retained>0)xgeEmojiPackFree(pack);
    return retained>0 ? retained-1:retained;
}
#endif
