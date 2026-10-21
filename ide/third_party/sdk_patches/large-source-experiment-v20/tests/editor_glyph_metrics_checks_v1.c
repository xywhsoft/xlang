#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long long metricAssertions,metricCases,metricScaleCases,metricAtlasCases,metricFailureCases;
#define METRIC_CHECK(e) do {++metricAssertions;if(!(e)){fprintf(stderr,"glyph metric check %d: %s\n",__LINE__,#e);exit(1);}}while(0)
static void metricPair(xge_font font,int glyph,xge_text_shape_metric_t* table) {
    xge_glyph_metrics_t expected;float advance=1234.5f,right=6789.25f;
    memset(&expected,0,sizeof(expected));
    METRIC_CHECK(xgeFontGlyphGetByIndex(font,glyph,&expected)==XGE_OK);
    METRIC_CHECK(__xgeTextShapeGlyphMetrics(font,glyph,&advance,&right,table)==XGE_OK);
    METRIC_CHECK(!memcmp(&advance,&expected.fAdvanceX,sizeof(advance)));
    METRIC_CHECK(!memcmp(&right,&expected.fX1,sizeof(right)));++metricCases;
}
static void verifyGlyphMetricCache(char** args) {
    xge_font_t fonts[3]={{0}},shared={0},empty={0};char path[4096];
    METRIC_CHECK(snprintf(path,sizeof(path),"%s/kern.ttf",args[1])>0);
    const char* paths[]={args[2],args[3],path};
    for(int f=0;f<3;++f)METRIC_CHECK(xgeFontLoad(&fonts[f],paths[f],20)==XGE_OK);
    METRIC_CHECK(xgeFontCreateSized(&shared,&fonts[2],32)==XGE_OK);
    METRIC_CHECK(__xgeFontInfo(&shared)==__xgeFontInfo(&fonts[2]));
    xge_text_shape_metric_t table[XGE_TEXT_SHAPE_METRIC_SLOTS]={0},before[XGE_TEXT_SHAPE_METRIC_SLOTS];
    unsigned long long hits=glyphMetricHits,atlas=glyphMetricAtlas;
    int maximumUnique=0,emptyBoxes=0;
    for(int phase=0;phase<2;++phase)for(int f=0;f<4;++f) {
        xge_font font=f==3?&shared:&fonts[f];const stbtt_fontinfo* info=__xgeFontInfo(font);
        METRIC_CHECK(info!=NULL);float originalScale=font->fScale;
        int glyphs[260],count=0;
        for(int code=0;code<260;++code) {
            int scalar=code<256?code:code==256?0x4e2d:code==257?0x6587:code==258?0x0301:0x1f642;
            int glyph=stbtt_FindGlyphIndex(info,scalar);int found=0;
            for(int i=0;i<count;++i)if(glyphs[i]==glyph)found=1;
            if(!found)glyphs[count++]=glyph;
        }
        if(count>maximumUnique)maximumUnique=count;
        METRIC_CHECK(count>0 && count<=260);
        if(phase==1) {
            for(int i=0;i<count && i<96;++i) {
                xge_glyph_t glyph;
                METRIC_CHECK(xgeFontGlyphAtlasGetByIndex(font,glyphs[i],&glyph)==XGE_OK);
            }
        }
        const float scales[]={originalScale,originalScale*0.75f,originalScale*1.25f,
            originalScale*2.0f,nextafterf(originalScale,0.0f),0.0f,-0.0f,-originalScale};
        for(unsigned int scale=0;scale<sizeof(scales)/sizeof(*scales);++scale) {
            font->fScale=scales[scale];
            for(int i=0;i<count;++i) {
                metricPair(font,glyphs[i],table);metricPair(font,glyphs[i],table);metricPair(font,glyphs[i],NULL);
            }
            ++metricScaleCases;
        }
        font->fScale=originalScale;
        if(phase==0) {
            for(int i=0;i<count;++i) {
                int x0,y0,x1,y1;if(!stbtt_GetGlyphBox(info,glyphs[i],&x0,&y0,&x1,&y1))++emptyBoxes;
            }
        } else {
            xge_glyph_cache_t* cached=__xgeFontGlyphFindByIndex(font,glyphs[0]);METRIC_CHECK(cached!=NULL);
            xge_glyph_t saved=cached->tGlyph;
            /* A live atlas branch must win over a previous face-metric hit. */
            cached->tGlyph.fAdvanceX+=1.25f;cached->tGlyph.fOffsetX+=0.5f;cached->tGlyph.iWidth+=2;
            metricPair(font,glyphs[0],table);cached->tGlyph=saved;++metricAtlasCases;
        }
    }
    METRIC_CHECK(maximumUnique>64 && emptyBoxes>0);
    memcpy(before,table,sizeof(table));
    for(int failure=0;failure<3;++failure) {
        float advance=1234.5f,right=6789.25f;
        xge_font font=failure==0?NULL:failure==1?&fonts[0]:&empty;
        int glyph=failure==1?-1:0;
        int expected=failure==2?XGE_ERROR_NOT_INITIALIZED:XGE_ERROR_INVALID_ARGUMENT;
        METRIC_CHECK(__xgeTextShapeGlyphMetrics(font,glyph,&advance,&right,table)==expected);
        METRIC_CHECK(advance==1234.5f && right==6789.25f && !memcmp(table,before,sizeof(table)));++metricFailureCases;
    }
    METRIC_CHECK(glyphMetricHits>hits && glyphMetricAtlas>atlas);
    xgeFontFree(&shared);for(int f=0;f<3;++f)xgeFontFree(&fonts[f]);
    printf("glyph metric comparison passed: %llu assertions; %llu metric cases; %llu live scale cases; %llu live atlas cases; %llu failure cases; %d distinct glyphs; %d empty boxes; %llu directly observed hits; %zu bytes call-local table\n",metricAssertions,metricCases,metricScaleCases,metricAtlasCases,metricFailureCases,maximumUnique,emptyBoxes,glyphMetricHits-hits,sizeof(table));fflush(stdout);
}
