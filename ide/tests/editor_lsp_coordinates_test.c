#include "editor/lsp_coordinates.h"
#include "protocol/lsp_wire.h"
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do {++checks;if(!(value)){fprintf(stderr,"editor_lsp_coordinates_test:%d: %s\n",__LINE__,#value);return 0;}}while(0)
static int compare(xui_code_document document,const char* expected)
{
    int size=(int)strlen(expected);uint32_t version=xuiCodeDocumentGetVersion(document);int undo=xuiCodeDocumentCanUndo(document),redo=xuiCodeDocumentCanRedo(document);
    CHECK(xuiCodeDocumentGetLength(document)==size);
    for(int at=-1;at<=size+1;++at) {
        int line=701,character=702,a=-1,b=-1;
        int reference=xideLspOffsetToPosition(expected,at,&a,&b),indexed=xideCodeDocumentOffsetToLsp(document,at,&line,&character);
        CHECK(reference==indexed);
        if(reference) {CHECK(line==a&&character==b);int offset=-1;CHECK(xideCodeDocumentLspToOffset(document,line,character,&offset)&&offset==at);}
        else CHECK(line==701&&character==702);
    }
    int count=xuiCodeDocumentGetLineCount(document);
    for(int line=-1;line<=count;++line) {
        int start=0,end=0,maximum=2,referenceLine=0;
        if(line>=0&&line<count) {CHECK(xuiCodeDocumentGetLineRange(document,line,&start,&end)==XUI_OK);CHECK(xideLspOffsetToPosition(expected,end,&referenceLine,&maximum));CHECK(referenceLine==line);}
        for(int character=-1;character<=maximum+1;++character) {
            int a=-1,offset=703;int reference=xideLspPositionToOffset(expected,line,character,&a),indexed=xideCodeDocumentLspToOffset(document,line,character,&offset);
            CHECK(reference==indexed);if(reference) CHECK(offset==a);else CHECK(offset==703);
        }
    }
    CHECK(xuiCodeDocumentGetVersion(document)==version&&xuiCodeDocumentCanUndo(document)==undo&&xuiCodeDocumentCanRedo(document)==redo);
    CHECK(!strcmp(xuiCodeDocumentGetText(document),expected));return 1;
}
static int run(void)
{
    xui_code_document document=NULL;CHECK(xuiCodeDocumentCreate(&document)==XUI_OK);
    struct sample {const char* input;const char* normalized;} samples[]={
        {"",""},{"abc","abc"},{"a\n","a\n"},{"\n\n","\n\n"},{"a\r\nb\rc\n","a\nb\nc\n"},
        {"A😀中\nB\r\nC\rD\n","A😀中\nB\nC\nD\n"},
        {"\xef\xbb\xbf" "αé😀\n","\xef\xbb\xbf" "αé😀\n"},
        {"中文 \t😀é\nlast 😀","中文 \t😀é\nlast 😀"}
    };
    for(size_t i=0;i<sizeof(samples)/sizeof(*samples);++i) CHECK(xuiCodeDocumentSetText(document,samples[i].input)==XUI_OK&&compare(document,samples[i].normalized));
    CHECK(xuiCodeDocumentSetText(document,"A😀中\n")==XUI_OK);
    CHECK(xuiCodeDocumentInsert(document,1,"😀z")==XUI_OK&&compare(document,"A😀z😀中\n"));
    CHECK(xuiCodeDocumentUndo(document)==XUI_OK&&compare(document,"A😀中\n"));
    CHECK(xuiCodeDocumentRedo(document)==XUI_OK&&compare(document,"A😀z😀中\n"));
    for(int boundary=4093;boundary<=4096;++boundary) {
        xstrbuf text;xrtStrBufInit(&text);int okay=1;
        for(int i=0;okay&&i<boundary;++i) okay=xrtStrBufAppend(&text,xrtStrView("a"));
        CHECK(okay&&xrtStrBufAppend(&text,xrtStrView("😀中é\nlast\n")));
        CHECK(xuiCodeDocumentSetText(document,text.Data)==XUI_OK&&compare(document,text.Data));xrtStrBufFree(&text);
    }
    for(int padding=0;padding<4;++padding) {
        xstrbuf text;xrtStrBufInit(&text);int okay=1;
        for(int i=0;okay&&i<padding;++i) okay=xrtStrBufAppend(&text,xrtStrView("a"));
        for(int i=0;okay&&i<450;++i) okay=xrtStrBufAppend(&text,xrtStrView("中😀é"));
        CHECK(okay&&xuiCodeDocumentSetText(document,text.Data)==XUI_OK&&compare(document,text.Data));xrtStrBufFree(&text);
    }
    int line=1,character=2,offset=3;
    CHECK(!xideCodeDocumentOffsetToLsp(NULL,0,&line,&character)&&line==1&&character==2);
    CHECK(!xideCodeDocumentOffsetToLsp(document,0,NULL,&character)&&character==2);
    CHECK(!xideCodeDocumentLspToOffset(NULL,0,0,&offset)&&offset==3);
    CHECK(!xideCodeDocumentLspToOffset(document,0,0,NULL));
    xuiCodeDocumentDestroy(document);return 1;
}
int main(void)
{
    if(!run()) return 1;
    printf("editor_lsp_coordinates_test passed: %u checks; actual SDK indexed UTF16/reference equivalence, scalar and EOL boundaries, chunk seams, edits/undo and failure atomicity.\n",checks);return 0;
}
