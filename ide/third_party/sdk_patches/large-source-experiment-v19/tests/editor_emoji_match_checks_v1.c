static unsigned long long memoAssertions,memoCases,memoLongCases,memoRevisionCases,memoIdentityCases,memoCursorCases,memoHitsObserved;
#define MEMO_CHECK(e) do {++memoAssertions;if(!(e)){fprintf(stderr,"emoji memo check %d: %s\n",__LINE__,#e);fflush(stderr);exit(1);}} while(0)
static int memoCompareMatch(xge_emoji_pack pack,const char* text,int size,int presentation,xge_text_emoji_match_cache_t* table,int expectedHit) {
 xge_emoji_match_t old={0},now={0};xge_emoji_read_extent_t a={0},b={0};unsigned long long hits=emojiMemoHits;int refs=pack->iRefCount;
 int expected=__xgeEmojiMatchForTextCursor(pack,text,text+size,presentation,&old,&a);
 int actual=__xgeTextMatchEmojiMemo(pack,text,text+size,presentation,&now,&b,table);
 MEMO_CHECK(actual==expected && !memcmp(&old,&now,sizeof(old)) && !memcmp(&a,&b,sizeof(a)));
 MEMO_CHECK(pack->iRefCount==refs && b.iRead>=0 && b.iRead<=size);
 if(expectedHit>=0)MEMO_CHECK(emojiMemoHits==hits+(unsigned)expectedHit);
 if(emojiMemoHits>hits){++memoHitsObserved;MEMO_CHECK(b.iRead<=16);}
 if(b.iRead>16)++memoLongCases;
 ++memoCases;return actual;
}
typedef struct memo_cursor_compare_t {const xge_glyph_run_t* reference;const xge_glyph_run_t* prefix;xge_internal_text_resume_t* resume;int at,started;} memo_cursor_compare_t;
static int memoCursorVisit(const xge_glyph_position_t* glyph,void* user) {
 memo_cursor_compare_t* p=user;
 if(!p->started){p->started=1;p->at=p->resume->iReplayGlyphIndex;MEMO_CHECK(p->at>=0 && p->at<=p->reference->iGlyphCount && p->at<=p->prefix->iGlyphCount);
  for(int i=0;i<p->at;++i)MEMO_CHECK(!memcmp(p->prefix->pGlyphs+i,p->reference->pGlyphs+i,sizeof(*glyph)));
 }
 MEMO_CHECK(p->at<p->reference->iGlyphCount && !memcmp(glyph,p->reference->pGlyphs+p->at,sizeof(*glyph)));++p->at;return XGE_OK;
}
static void memoCursorCase(xge_font font,xge_emoji_pack pack,const char* before,int bn,const char* after,int an,int expectResume,int boundary) {
 xge_text_shape_desc_t d={0};d.iSize=sizeof(d);d.pFont=font;d.pEmojiPack=pack;d.sText=before;d.iTextSize=bn;d.iFlags=XGE_TEXT_SHAPE_DEFAULT;d.fEmojiScale=1.25f;d.iEmojiLinePolicy=XGE_EMOJI_LINE_EXPAND;
 xge_glyph_run_t original={0},captured={0},reference={0},now={0};xge_internal_text_cursor *old=NULL,*fresh=NULL;xge_internal_text_resume_t info={0};int refs=font->iRefCount,packRefs=pack->iRefCount;
 MEMO_CHECK(diagOldTextShape(&d,&original)==XGE_OK);
 memo_cursor_compare_t c={&original,&original,&info,0,0};MEMO_CHECK(xgeInternalTextShapeVisitCached(&d,&captured,memoCursorVisit,&c,NULL,0,&old,&info)==XGE_OK && old && c.at==original.iGlyphCount);
 MEMO_CHECK(captured.fWidth==original.fWidth && captured.fHeight==original.fHeight && captured.fAscent==original.fAscent && captured.fDescent==original.fDescent && captured.iGlyphCount==original.iGlyphCount);xgeGlyphRunFree(&captured);
 int common=0;while(common<bn && common<an && before[common]==after[common])++common;d.sText=after;d.iTextSize=an;
 int expected=diagOldTextShape(&d,&reference);c=(memo_cursor_compare_t){&reference,&original,&info,0,0};int actual=xgeInternalTextShapeVisitCached(&d,&now,memoCursorVisit,&c,old,common,&fresh,&info);MEMO_CHECK(actual==expected);
 if(actual==XGE_OK){MEMO_CHECK(fresh && c.at==reference.iGlyphCount && now.fWidth==reference.fWidth && now.fHeight==reference.fHeight && now.fAscent==reference.fAscent && now.fDescent==reference.fDescent && now.iGlyphCount==reference.iGlyphCount);
  if(expectResume)MEMO_CHECK(info.iTextOffset>0 && info.iReplayGlyphIndex>0);
  if(boundary>=0)MEMO_CHECK(info.iTextOffset<=boundary);
 }else MEMO_CHECK(!c.started && !fresh);
 xgeGlyphRunFree(&now);xgeGlyphRunFree(&reference);xgeGlyphRunFree(&original);xgeInternalTextCursorFree(old);xgeInternalTextCursorFree(fresh);MEMO_CHECK(font->iRefCount==refs && pack->iRefCount==packRefs);++memoCursorCases;
}
static void verifyEmojiMemo(char** args) {
 xge_text_emoji_match_cache_t table[XGE_TEXT_EMOJI_MATCH_SLOTS]={0};MEMO_CHECK(sizeof(table)<=8192 && XGE_TEXT_EMOJI_MATCH_SLOTS==64 && XGE_TEXT_EMOJI_MATCH_PREFIX==16);
 xge_emoji_pack pack=NULL,other=NULL;MEMO_CHECK(xgeEmojiPackCreate(&pack)==XGE_OK && xgeEmojiPackCreate(&other)==XGE_OK);
 const char* svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><rect width='32' height='32' fill='#09a'/></svg>";
 xge_emoji_metrics_t em={0};em.iSize=sizeof(em);em.fAdvanceEm=em.fWidthEm=em.fHeightEm=1;em.fBaselineRatio=.9f;uint32_t id=0;
 const char* entries[]={"a","abcdefghijklmnop","abcdefghijklmnopq","😀","👩","👩‍💻","1️⃣"};
 for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);++i)MEMO_CHECK(xgeEmojiPackAddSvgMemory(pack,entries[i],svg,(int)strlen(svg),&em,&id)==XGE_OK);
 MEMO_CHECK(memoCompareMatch(pack,"abcdefghijklmnop",16,0,table,0)==XGE_OK);
 MEMO_CHECK(memoCompareMatch(pack,"abcdefghijklmnop",16,0,table,1)==XGE_OK);
 MEMO_CHECK(memoCompareMatch(pack,"abcdefghijklmnopq",17,0,table,0)==XGE_OK);
 MEMO_CHECK(memoCompareMatch(pack,"abcdefghijklmnopq",17,0,table,0)==XGE_OK);
 MEMO_CHECK(memoCompareMatch(pack,"abcdefghijklmnop",16,0,table,1)==XGE_OK);
 const char* corpora[]={"a 01234567890123456789","a︎ 01234567890123456789","a️ 01234567890123456789","a‍ 01234567890123456789","👩‍💻 abcdefghijklmnopq","👩🏽 abcdefghijklmnopq","😀 01234567890123456789","😀︎ 01234567890123456789","😀️ 01234567890123456789","1️⃣ abcdefghijklmnopq","🏽🏽🏽🏽🏽👩 0123456789","xyz 01234567890123456789"};
 /* Each shape call fixes its presentation; the table does not outlive it. */
 for(int presentation=0;presentation<4;++presentation){memset(table,0,sizeof(table));for(size_t i=0;i<sizeof(corpora)/sizeof(corpora[0]);++i)for(int repeat=0;repeat<2;++repeat)memoCompareMatch(pack,corpora[i],(int)strlen(corpora[i]),presentation,table,-1);}
 char text[80];memset(table,0,sizeof(table));strcpy(text,"missing 01234567890123456789");MEMO_CHECK(memoCompareMatch(pack,text,(int)strlen(text),0,table,0)==XGE_ERROR_NOT_FOUND);MEMO_CHECK(memoCompareMatch(pack,text,(int)strlen(text),0,table,1)==XGE_ERROR_NOT_FOUND);
 MEMO_CHECK(xgeEmojiPackAddSvgMemory(pack,"m",svg,(int)strlen(svg),&em,&id)==XGE_OK);MEMO_CHECK(memoCompareMatch(pack,text,(int)strlen(text),0,table,0)==XGE_OK);MEMO_CHECK(memoCompareMatch(pack,text,(int)strlen(text),0,table,1)==XGE_OK);++memoRevisionCases;
 em.fAdvanceEm=em.fWidthEm=1.7f;MEMO_CHECK(xgeEmojiPackAddSvgMemory(pack,"m",svg,(int)strlen(svg),&em,&id)==XGE_OK);MEMO_CHECK(memoCompareMatch(pack,text,(int)strlen(text),0,table,0)==XGE_OK);++memoRevisionCases;
 xge_emoji_metrics_t bad=em;bad.iSize=1;MEMO_CHECK(xgeEmojiPackAddSvgMemory(pack,"m",svg,(int)strlen(svg),&bad,&id)==XGE_ERROR_INVALID_ARGUMENT);MEMO_CHECK(memoCompareMatch(pack,text,(int)strlen(text),0,table,0)==XGE_OK);++memoRevisionCases;
 em.fWidthEm=.7f;MEMO_CHECK(xgeEmojiPackAddSvgMemory(other,"m",svg,(int)strlen(svg),&em,&id)==XGE_OK);MEMO_CHECK(memoCompareMatch(other,text,(int)strlen(text),0,table,0)==XGE_OK);MEMO_CHECK(memoCompareMatch(pack,text,(int)strlen(text),0,table,0)==XGE_OK);++memoIdentityCases;
 uint64_t revision=pack->iShapeRevision;pack->iShapeRevision=UINT64_MAX;unsigned long long bypass=emojiMemoBypass;memoCompareMatch(pack,text,(int)strlen(text),0,table,0);memoCompareMatch(pack,text,(int)strlen(text),0,table,0);MEMO_CHECK(emojiMemoBypass==bypass+2);pack->iShapeRevision=revision;++memoRevisionCases;
 /* More distinct successful sequences than slots force replacement collisions. */
 em.fWidthEm=em.fAdvanceEm=1;for(int i=0;i<128;++i){char name[16];snprintf(name,sizeof(name),"X%03d",i);em.iFlags=i&1?XGE_EMOJI_ENTRY_TEXT_DEFAULT:0;MEMO_CHECK(xgeEmojiPackAddSvgMemory(pack,name,svg,(int)strlen(svg),&em,&id)==XGE_OK);}
 memset(table,0,sizeof(table));for(int round=0;round<3;++round)for(int i=0;i<128;++i){snprintf(text,sizeof(text),"X%03d--012345678901234567890",round&1?127-i:i);memoCompareMatch(pack,text,(int)strlen(text),0,table,-1);}
 xge_font_t font={0};MEMO_CHECK(xgeFontLoad(&font,args[2],20)==XGE_OK);char *before=malloc(32768),*after=malloc(32768);MEMO_CHECK(before && after);
 const char* motif="X000--😀 0123456789 ";size_t n=strlen(motif);int used=0;while(used+(int)n<=24576){memcpy(before+used,motif,n);used+=(int)n;}before[used]=0;
 memcpy(after,before,(size_t)used+1);after[used-1]='V';memoCursorCase(&font,pack,before,used,after,used,1,used-1);
 memcpy(after,before,(size_t)used);after[used]=(char)0xff;after[used+1]=0;memoCursorCase(&font,pack,before,used,after,used+1,0,-1);
 memset(before,'Q',20480);memcpy(before+20480,"😀",4);before[20484]=0;memcpy(after,before,20484);memcpy(after+20484,"︎",3);after[20487]=0;memoCursorCase(&font,pack,before,20484,after,20487,1,20480);
 memcpy(after,before,20484);memcpy(after+20484,"️",3);after[20487]=0;memoCursorCase(&font,pack,before,20484,after,20487,1,20480);
 memset(before,'Q',24576);memcpy(before+8190,"abcdefghijklmnopq!",18);before[24576]=0;memcpy(after,before,24577);after[8190+16]='Z';memoCursorCase(&font,pack,before,24576,after,24576,1,8190);
 memset(before,'Q',8188);memcpy(before+8188,"👩",4);before[8192]=0;memcpy(after,before,8192);memcpy(after+8192,"‍💻",7);after[8199]=0;memoCursorCase(&font,pack,before,8192,after,8199,1,8188);
 free(before);free(after);xgeFontFree(&font);xgeEmojiPackFree(pack);xgeEmojiPackFree(other);
 MEMO_CHECK(memoLongCases>=3 && memoHitsObserved>10 && memoRevisionCases==4 && memoIdentityCases==1 && memoCursorCases==6 && emojiMemoHits>1000);
 printf("emoji memo comparison passed: %llu assertions; %llu match cases; %llu long dependencies; %llu revision cases; %llu pack identity cases; %llu cursor cases; %llu directly observed hits; %zu bytes call-local table\n",memoAssertions,memoCases,memoLongCases,memoRevisionCases,memoIdentityCases,memoCursorCases,memoHitsObserved,sizeof(table));fflush(stdout);
}
