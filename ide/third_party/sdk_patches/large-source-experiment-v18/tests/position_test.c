#include "models.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static unsigned long long checks,calls;
static xui_code_document a,b;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"position equivalence %d: %s\n",__LINE__,#x); exit(1); } } while(0)
int countedNextRead(xui_internal_text_read_proc read,void* user,int length,int offset) {
    ++calls;return xuiInternalTextGraphemeNextRead(read,user,length,offset);
}
static uint32_t randomState=0x35ac72e1u;
static uint32_t randomValue(void) {randomState=randomState*1664525u+1013904223u;return randomState;}
static void state(void) {
    int n=old_xuiCodeDocumentGetLength(a);CHECK(n==new_xuiCodeDocumentGetLength(b));
    CHECK(old_xuiCodeDocumentGetLineCount(a)==new_xuiCodeDocumentGetLineCount(b));
    CHECK(old_xuiCodeDocumentGetVersion(a)==new_xuiCodeDocumentGetVersion(b));
    CHECK(old_xuiCodeDocumentGetChangeVersion(a)==new_xuiCodeDocumentGetChangeVersion(b));
    CHECK(old_xuiCodeDocumentGetDirty(a)==new_xuiCodeDocumentGetDirty(b));
    CHECK(old_xuiCodeDocumentCanUndo(a)==new_xuiCodeDocumentCanUndo(b));CHECK(old_xuiCodeDocumentCanRedo(a)==new_xuiCodeDocumentCanRedo(b));
    CHECK(!strcmp(old_xuiCodeDocumentGetText(a),new_xuiCodeDocumentGetText(b)));
    for(int i=0;i<n;++i) {char x=0,y=0;CHECK(old_xuiCodeDocumentGetByte(a,i,&x)==new_xuiCodeDocumentGetByte(b,i,&y) && x==y);}
}
static void query(int offset) {
    int line=123,column=456,l2=123,c2=456,p=789,p2=789;
    CHECK(old_xuiCodeDocumentOffsetToLineColumn(a,offset,&line,&column)==new_xuiCodeDocumentOffsetToLineColumn(b,offset,&l2,&c2));CHECK(line==l2 && column==c2);
    CHECK(old_xuiCodeDocumentLineColumnToOffset(a,line,column,&p)==new_xuiCodeDocumentLineColumnToOffset(b,l2,c2,&p2) && p==p2);
    CHECK(old_xuiCodeDocumentOffsetToLineColumn(a,offset,&line,&column)==new_xuiCodeDocumentOffsetToLineColumn(b,offset,&l2,&c2) && line==l2 && column==c2);
    for(int kind=0;kind<4;++kind) {
        int x=11,y=22,x2=11,y2=22;
        CHECK(old_xuiCodeDocumentOffsetToLineColumn(a,offset,kind&1 ? &x : NULL,kind&2 ? &y : NULL)==new_xuiCodeDocumentOffsetToLineColumn(b,offset,kind&1 ? &x2 : NULL,kind&2 ? &y2 : NULL));CHECK(x==x2 && y==y2);
    }
    int alias=123,alias2=123;CHECK(old_xuiCodeDocumentOffsetToLineColumn(a,offset,&alias,&alias)==new_xuiCodeDocumentOffsetToLineColumn(b,offset,&alias2,&alias2) && alias==alias2);
}
static void conversions(void) {
    int n=old_xuiCodeDocumentGetLength(a),lines=old_xuiCodeDocumentGetLineCount(a);
    for(int offset=-2;offset<n+3;++offset) query(offset);
    for(int line=-1;line<=lines;++line) {
        for(int column=-2;column<24;++column) {
            int x=17,y=17;CHECK(old_xuiCodeDocumentLineColumnToOffset(a,line,column,&x)==new_xuiCodeDocumentLineColumnToOffset(b,line,column,&y) && x==y);
            CHECK(old_xuiCodeDocumentLineColumnToOffset(a,line,column,NULL)==new_xuiCodeDocumentLineColumnToOffset(b,line,column,NULL));
        }
        int x=17,y=17;CHECK(old_xuiCodeDocumentLineColumnToOffset(a,line,INT_MAX,&x)==new_xuiCodeDocumentLineColumnToOffset(b,line,INT_MAX,&y) && x==y);
    }
}
static void text(const char* s) {CHECK(old_xuiCodeDocumentSetText(a,s)==new_xuiCodeDocumentSetText(b,s));state();conversions();}
typedef struct observer {int count,isNew,line,column;uint32_t version;xui_code_range_t range;} observer;
static void changed(xui_code_document d,xui_code_range_t r,uint32_t v,void* user) {
    observer* o=user;++o->count;o->range=r;o->version=v;
    int n=o->isNew ? new_xuiCodeDocumentGetLength(d) : old_xuiCodeDocumentGetLength(d);
    int result=o->isNew ? new_xuiCodeDocumentOffsetToLineColumn(d,n,&o->line,&o->column) : old_xuiCodeDocumentOffsetToLineColumn(d,n,&o->line,&o->column);
    CHECK(result==XUI_OK);
}
int main(void) {
    CHECK(old_xuiCodeDocumentCreate(&a)==XUI_OK && new_xuiCodeDocumentCreate(&b)==XUI_OK);
    const char* cases[]={"","abc","中é👨‍👩‍👧‍👦\tZ\r\n第二行\r\n","A\rB\nC\r\nD","🇨🇳🇫🇷🇺🇸","각각","क्ष क्‍ष","مرحبا אבג á","a\xCC\x81\xCC\x82\xCC\x83","\n\n\n","👩🏽‍💻\t🏳️‍🌈\n尾"};
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i) text(cases[i]);
    const char nul[]={'A',0,'B','\n',(char)0xe4,(char)0xb8,(char)0xad,0};
    CHECK(old_xuiCodeDocumentSetTextLength(a,nul,7)==new_xuiCodeDocumentSetTextLength(b,nul,7));state();conversions();
    text("中é😀\tXYZ\n第二行");int n=old_xuiCodeDocumentGetLength(a),line=0,col=0,at=0;
    CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,n,&line,&col)==XUI_OK);calls=0;
    for(int i=0;i<32;++i) {CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,n,&line,&col)==XUI_OK);CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,0,&at,NULL)==XUI_OK);CHECK(new_xuiCodeDocumentLineColumnToOffset(b,line,col,&at)==XUI_OK && at==n);}
    CHECK(calls==0);CHECK(old_gap(a,2)==XUI_OK && new_gap(b,2)==XUI_OK);calls=0;CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,n,&line,&col)==XUI_OK && calls==0);state();
    CHECK(old_raw(a,0,3,"abc")==new_raw(b,0,3,"abc"));calls=0;CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,8,&line,&col)==XUI_OK && calls>0);state();conversions();
    CHECK(old_owned(a,"中é\n尾")==new_owned(b,"中é\n尾"));calls=0;CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,6,&line,&col)==XUI_OK && calls>0);state();conversions();
    text("中Z");old_setVersion(a,0);new_setVersion(b,0);CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,3,&line,&col)==XUI_OK && col==1);
    old_setVersion(a,UINT32_MAX);new_setVersion(b,UINT32_MAX);CHECK(old_xuiCodeDocumentReplace(a,0,3,"abc")==new_xuiCodeDocumentReplace(b,0,3,"abc"));CHECK(new_xuiCodeDocumentGetVersion(b)==0);calls=0;CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,3,&line,&col)==XUI_OK && col==3 && calls>0);state();conversions();
    text("ab中😀\nsecond\n");observer x={0},y={0};y.isNew=1;
    CHECK(old_xuiCodeDocumentSetChangeCallback(a,changed,&x)==new_xuiCodeDocumentSetChangeCallback(b,changed,&y));
    const char* inserts[]={"a","中","é","😀","👨‍👩‍👧‍👦","\n","\r\n","\t","क्ष","","🇨🇳"};
    for(int step=0;step<480;++step) {
        unsigned kind=randomValue()%7;n=old_xuiCodeDocumentGetLength(a);
        if(n>768) {CHECK(old_xuiCodeDocumentSetText(a,"reset\n中")==new_xuiCodeDocumentSetText(b,"reset\n中"));}
        else if(kind<4) {
            int start=(int)(randomValue()%(unsigned)(n+1)),end=(int)(randomValue()%(unsigned)(n+1));
            const char* insert=inserts[randomValue()%11];
            CHECK(old_xuiCodeDocumentReplace(a,start,end,insert)==new_xuiCodeDocumentReplace(b,start,end,insert));
        } else if(kind==4) CHECK(old_xuiCodeDocumentUndo(a)==new_xuiCodeDocumentUndo(b));
        else if(kind==5) CHECK(old_xuiCodeDocumentRedo(a)==new_xuiCodeDocumentRedo(b));
        else {
            CHECK(old_xuiCodeDocumentBeginEdit(a)==new_xuiCodeDocumentBeginEdit(b));
            CHECK(old_xuiCodeDocumentInsert(a,0,"中\n")==new_xuiCodeDocumentInsert(b,0,"中\n"));
            CHECK(old_xuiCodeDocumentInsert(a,old_xuiCodeDocumentGetLength(a),"尾😀")==new_xuiCodeDocumentInsert(b,new_xuiCodeDocumentGetLength(b),"尾😀"));
            CHECK(old_xuiCodeDocumentEndEdit(a)==new_xuiCodeDocumentEndEdit(b));
        }
        state();conversions();CHECK(x.count==y.count && x.version==y.version && x.range.iStart==y.range.iStart && x.range.iEnd==y.range.iEnd && x.line==y.line && x.column==y.column);
    }
    CHECK(old_xuiCodeDocumentSetChangeCallback(a,NULL,NULL)==new_xuiCodeDocumentSetChangeCallback(b,NULL,NULL));text("unchanged 中");
    n=old_xuiCodeDocumentGetLength(a);CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,n,&line,&col)==XUI_OK);
    const char bad[]={(char)0xff,0};CHECK(old_xuiCodeDocumentSetText(a,bad)==new_xuiCodeDocumentSetText(b,bad));calls=0;CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,n,&line,&col)==XUI_OK && calls==0);state();
    CHECK(old_xuiCodeDocumentSetDirty(a,0)==new_xuiCodeDocumentSetDirty(b,0));calls=0;CHECK(new_xuiCodeDocumentOffsetToLineColumn(b,n,&line,&col)==XUI_OK && calls==0);
    CHECK(old_xuiCodeDocumentLineColumnToOffset(NULL,0,0,&at)==new_xuiCodeDocumentLineColumnToOffset(NULL,0,0,&at));
    CHECK(old_xuiCodeDocumentOffsetToLineColumn(NULL,0,&line,&col)==new_xuiCodeDocumentOffsetToLineColumn(NULL,0,&line,&col));
    old_xuiCodeDocumentDestroy(a);new_xuiCodeDocumentDestroy(b);a=b=NULL;
    printf("position cache equivalence passed: %llu assertions; Unicode byte positions and inverse, optional/alias outputs, pending-line shifts, raw/owned same-version mutation, version wrap, callback observations, grouped edits/undo/redo, rejected UTF-8 and 32 repeated forward/inverse conversions without grapheme calls\n",checks);return 0;
}