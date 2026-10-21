#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long long kernAssertions,kernPairs,kernMutationCases,kernMutationPositions;
#define KERN_CHECK(e) do {++kernAssertions;if(!(e)){fprintf(stderr,"kern check %d: %s\n",__LINE__,#e);exit(1);}}while(0)
typedef struct kern_mutation_state_t {
    xge_font primary,fallback;
    float primaryScale,fallbackScale;
    xge_glyph_position_t* positions;
    int at,capacity,compare;
} kern_mutation_state_t;
static int kernMutatingVisitor(const xge_glyph_position_t* position,void* user) {
    kern_mutation_state_t* state=user;
    KERN_CHECK(state->at<state->capacity);
    if(state->compare){KERN_CHECK(!memcmp(position,&state->positions[state->at],sizeof(*position)));++kernMutationPositions;}
    else state->positions[state->at]=*position;
    ++state->at;
    const float scales[]={1.0f,0.75f,1.25f,2.0f};
    state->primary->fScale=state->primaryScale*scales[state->at%4];
    state->fallback->fScale=state->fallbackScale*scales[(state->at+1)%4];
    return XGE_OK;
}
static void verifyKernCache(char** args) {
    xge_font_t fonts[3]={{0}},shared={0};char path[4096];
    KERN_CHECK(snprintf(path,sizeof(path),"%s/kern.ttf",args[1])>0);
    const char* paths[]={args[2],args[3],path};
    for(int f=0;f<3;++f)KERN_CHECK(xgeFontLoad(&fonts[f],paths[f],20)==XGE_OK);
    KERN_CHECK(xgeFontCreateSized(&shared,&fonts[2],32)==XGE_OK);
    KERN_CHECK(__xgeFontInfo(&shared)==__xgeFontInfo(&fonts[2]));
    xge_text_shape_kern_t table[XGE_TEXT_SHAPE_KERN_SLOTS]={0};
    KERN_CHECK(sizeof(table)==64*sizeof(xge_text_shape_kern_t));
    int nonzero=0;
    for(int round=0;round<2;++round)for(int f=0;f<4;++f) {
        const stbtt_fontinfo* info=__xgeFontInfo(f==3?&shared:&fonts[f]);KERN_CHECK(info!=NULL);
        for(int left=0;left<128;++left)for(int right=0;right<128;++right) {
            int l=stbtt_FindGlyphIndex(info,left),r=stbtt_FindGlyphIndex(info,right);
            int expected=stbtt_GetGlyphKernAdvance(info,l,r);if(expected)++nonzero;
            KERN_CHECK(__xgeTextShapeKernAdvance(info,l,r,table)==expected);
            KERN_CHECK(__xgeTextShapeKernAdvance(info,l,r,table)==expected);
            KERN_CHECK(__xgeTextShapeKernAdvance(info,l,r,NULL)==expected);++kernPairs;
        }
    }
    KERN_CHECK(nonzero>0);
    const int lengths[]={255,256,257,8192};char text[8193];const char* motif="AVATAR To WA. 中文 ";int m=(int)strlen(motif);
    xge_glyph_position_t* positions=calloc(8193,sizeof(*positions));KERN_CHECK(positions);
    fonts[2].pFallback=&fonts[1];
    for(int flag=0;flag<4;++flag)for(int length=0;length<4;++length) {
        int n=lengths[length],used=0;while(used+m<=n){memcpy(text+used,motif,(size_t)m);used+=m;}memset(text+used,'A',(size_t)(n-used));text[n]=0;
        xge_text_shape_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=&fonts[2];desc.sText=text;desc.iTextSize=n;desc.iFlags=(uint32_t)flag;
        kern_mutation_state_t state={&fonts[2],&fonts[1],fonts[2].fScale,fonts[1].fScale,positions,0,8193,0};
        int primaryRefs=fonts[2].iRefCount,fallbackRefs=fonts[1].iRefCount;
        xge_glyph_run_t old={0},now={0};
        KERN_CHECK(kernPreviousxgeInternalTextShapeVisit(&desc,&old,kernMutatingVisitor,&state)==XGE_OK);
        int count=state.at;state.primary->fScale=state.primaryScale;state.fallback->fScale=state.fallbackScale;state.at=0;state.compare=1;
        KERN_CHECK(xgeInternalTextShapeVisit(&desc,&now,kernMutatingVisitor,&state)==XGE_OK);
        KERN_CHECK(state.at==count && old.iGlyphCount==now.iGlyphCount && old.pGlyphs==NULL && now.pGlyphs==NULL);
        KERN_CHECK(old.iFlags==now.iFlags && old.iTextSize==now.iTextSize && old.fWidth==now.fWidth && old.fHeight==now.fHeight && old.fAscent==now.fAscent && old.fDescent==now.fDescent && old.fLineHeight==now.fLineHeight);
        state.primary->fScale=state.primaryScale;state.fallback->fScale=state.fallbackScale;
        xgeGlyphRunFree(&old);xgeGlyphRunFree(&now);KERN_CHECK(fonts[2].iRefCount==primaryRefs && fonts[1].iRefCount==fallbackRefs);++kernMutationCases;
    }
    fonts[2].pFallback=NULL;free(positions);xgeFontFree(&shared);for(int f=0;f<3;++f)xgeFontFree(&fonts[f]);
    printf("kern cache comparison passed: %llu assertions; %llu raw pairs; %llu mutation cases; %llu mutation positions; %zu bytes call-local table; %d nonzero kerning pairs\n",kernAssertions,kernPairs,kernMutationCases,kernMutationPositions,sizeof(table),nonzero);fflush(stdout);
}
