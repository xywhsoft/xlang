#include "foreign_xge3_text_resources.h"
#include <stdlib.h>
#include <xrt_decl.h>
#include <stdio.h>
static xmemdebugsnapshot costBefore;
const char* fixtureXgeTextPath(void)
{const char* path=getenv("XLANG_XGE_FONT_FIXTURE");return path ? path:"";}
int fixtureXgeTextHotBegin(void)
{if(!xrtMemDebugEnabled() && !xrtMemDebugEnable(true))return 0;return xrtMemDebugFailAfter(0);}
int fixtureXgeTextHotEnd(void)
{int ok=!xrtMemDebugFailTriggered();xrtMemDebugFailClear();return ok;}
void fixtureXgeTextCostBegin(void){xrtMemDebugSnapshot(&costBefore);}
void fixtureXgeTextCostReport(int stage)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("text retained stage %d: %lld allocations, %lld bytes\n",stage,
  (long long)after.LiveCount-(long long)costBefore.LiveCount,
  (long long)after.LiveBytes-(long long)costBefore.LiveBytes);
}
int fixtureXgeTextNativeCosts(xge_font font)
{
 xge_text_shape_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=font;desc.sText="ffi";
 desc.iTextSize=3;desc.iFlags=1;desc.fEmojiScale=1;
 xge_glyph_run_t* run=NULL;int status=xgeTextShapeOwned(&run,&desc);if(status)return status;
 xgeGlyphRunFree(run);run=NULL;fixtureXgeTextCostBegin();status=xgeTextShapeOwned(&run,&desc);
 if(status)return status;
 fixtureXgeTextCostReport(-1);
 fixtureXgeTextCostBegin();const xge_glyph_position_t* glyphs=NULL;size_t count=0;
 status=xgeGlyphRunGetGlyphs(run,&glyphs,&count);if(status){xgeGlyphRunFree(run);return status;}
 if(!glyphs || !count || xgeGlyphRunAddRef(run)<=0){xgeGlyphRunFree(run);return XGE_ERROR_INVALID_STATE;}
 fixtureXgeTextCostReport(-3);xgeGlyphRunFree(run);xgeGlyphRunFree(run);
 xge_glyph_bitmap_resource bitmap=NULL;status=xgeFontGlyphRasterizeOwned(&bitmap,font,65);if(status)return status;
 xgeGlyphBitmapResourceFree(bitmap);bitmap=NULL;fixtureXgeTextCostBegin();status=xgeFontGlyphRasterizeOwned(&bitmap,font,65);
 if(status)return status;
 fixtureXgeTextCostReport(-2);fixtureXgeTextCostBegin();const unsigned char* pixels=NULL;count=0;
 status=xgeGlyphBitmapResourceGetPixels(bitmap,&pixels,&count);if(status){xgeGlyphBitmapResourceFree(bitmap);return status;}
 if(!pixels || !count || xgeGlyphBitmapResourceAddRef(bitmap)<=0){xgeGlyphBitmapResourceFree(bitmap);return XGE_ERROR_INVALID_STATE;}
 fixtureXgeTextCostReport(-4);xgeGlyphBitmapResourceFree(bitmap);xgeGlyphBitmapResourceFree(bitmap);return 0;
}
