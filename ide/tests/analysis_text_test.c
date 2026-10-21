#include "analysis/text.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int checks;
#define CHECK(call) do { ++checks; if(!(call)) { fprintf(stderr,"analysis_text_test:%d: %s\n",__LINE__,#call); return 1; } } while(0)
static xls_text_edit edit(size_t sl,size_t sc,size_t el,size_t ec,const char* replacement)
{
    xls_text_edit value={.text=replacement,.length=strlen(replacement),.hasRange=1,
        .start={sl,sc},.end={el,ec}};
    return value;
}

int main(void)
{
    enum xls_text_error error=XLS_TEXT_OK;
    const char* original="a😀中\r\nxy\n";
    xls_text* base=xlsTextCreate(original,strlen(original),4096,&error);
    size_t offset=999;
    CHECK(base && error==XLS_TEXT_OK && xlsTextLines(base)==3);
    CHECK(xlsTextOffset(base,(xls_position){0,1},&offset) && offset==1);
    CHECK(xlsTextOffset(base,(xls_position){0,3},&offset) && offset==5);
    CHECK(!xlsTextOffset(base,(xls_position){0,2},&offset));
    CHECK(xlsTextOffset(base,(xls_position){0,999},&offset) && offset==8);
    CHECK(xlsTextOffset(base,(xls_position){2,0},&offset) && offset==strlen(original));
    CHECK(!xlsTextOffset(base,(xls_position){3,0},&offset));
    xls_text_edit change=edit(0,1,0,3,"Q"); change.hasRangeLength=1; change.rangeLength=2;
    xls_text* value=xlsTextApply(base,&change,1,4096,&error);
    CHECK(value && !strcmp(xlsTextData(value),"aQ中\r\nxy\n"));
    CHECK(!strcmp(xlsTextData(base),original)); xlsTextRelease(value);
    change.rangeLength=1;
    CHECK(!xlsTextApply(base,&change,1,4096,&error) && error==XLS_TEXT_RANGE);
    xls_text_edit batch[]={edit(0,0,0,1,"hi\n"),edit(1,0,1,2,"Z")};
    value=xlsTextApply(base,batch,2,4096,&error);
    CHECK(value && !strcmp(xlsTextData(value),"hi\nZ中\r\nxy\n") && xlsTextLines(value)==4);
    xlsTextRelease(value);
    batch[1]=edit(1,1,1,2,"Z");
    CHECK(!xlsTextApply(base,batch,2,4096,&error) && error==XLS_TEXT_RANGE);
    CHECK(!strcmp(xlsTextData(base),original));
    change=edit(0,4,1,1,""); change.hasRangeLength=1; change.rangeLength=3;
    value=xlsTextApply(base,&change,1,4096,&error);
    CHECK(value && !strcmp(xlsTextData(value),"a😀中y\n")); xlsTextRelease(value);
    change=edit(0,3,0,1,"");
    CHECK(!xlsTextApply(base,&change,1,4096,&error) && error==XLS_TEXT_RANGE);
    change=edit(0,0,0,0,"too long");
    CHECK(!xlsTextApply(base,&change,1,strlen(original),&error) && error==XLS_TEXT_LIMIT);
    change=edit(0,0,0,0,"\xc0\x80");
    CHECK(!xlsTextApply(base,&change,1,4096,&error) && error==XLS_TEXT_ENCODING);
    change=(xls_text_edit){.text="a\0b",.length=3};
    CHECK(!xlsTextApply(base,&change,1,4096,&error) && error==XLS_TEXT_ENCODING);
    CHECK(!xlsTextApply(base,&change,1025,4096,&error) && error==XLS_TEXT_LIMIT);
    value=xlsTextApply(base,NULL,0,4096,&error);
    CHECK(value==base); xlsTextRelease(value);
    xls_text_edit mixed[]={{.text="first\nsecond",.length=12},edit(1,0,1,6,"下一行")};
    value=xlsTextApply(base,mixed,2,4096,&error);
    CHECK(value && !strcmp(xlsTextData(value),"first\n下一行")); xlsTextRelease(value);
    xlsTextRelease(base);
    base=xlsTextCreate("a\nb",3,4096,&error);
    change=edit(0,1,0,1,"\r"); value=xlsTextApply(base,&change,1,4096,&error);
    CHECK(value && !strcmp(xlsTextData(value),"a\r\nb") && xlsTextLines(value)==2);
    xlsTextRelease(base); base=value;
    change=edit(0,1,1,0,""); value=xlsTextApply(base,&change,1,4096,&error);
    CHECK(value && !strcmp(xlsTextData(value),"ab") && xlsTextLines(value)==1);
    xlsTextRelease(base); xlsTextRelease(value);
    base=xlsTextCreate(NULL,0,0,&error);
    CHECK(base && xlsTextLines(base)==1 && !xlsTextSize(base));
    change=(xls_text_edit){.text="",.length=0}; value=xlsTextApply(base,&change,1,0,&error);
    CHECK(value && !xlsTextSize(value)); xlsTextRelease(base); xlsTextRelease(value);
    CHECK(!xlsTextCreate("\xed\xa0\x80",3,4096,&error) && error==XLS_TEXT_ENCODING);
    base=xlsTextCreate("\xef\xbb\xbf中\rZ",8,4096,&error);
    CHECK(base && xlsTextLines(base)==2);
    CHECK(xlsTextOffset(base,(xls_position){0,1},&offset) && offset==3);
    CHECK(xlsTextOffset(base,(xls_position){1,0},&offset) && offset==7);
    xlsTextRelease(base);

    /* Differential edit replay: compare each retained snapshot to an independent
       flat reference, including growth, shrink, replacement and empty sources. */
    char reference[4096]="seed",next[4096]; uint32_t random=17;
    base=xlsTextCreate(reference,strlen(reference),4095,&error);
    for(int i=0;i<500;++i) {
        size_t size=strlen(reference);
        random=random*1664525u+1013904223u; size_t start=random%(size+1);
        random=random*1664525u+1013904223u; size_t end=start+random%(size-start+1);
        random=random*1664525u+1013904223u; size_t inserted=random%7;
        const char* letters="abcdef";
        memcpy(next,reference,start); memcpy(next+start,letters,inserted);
        memcpy(next+start+inserted,reference+end,size-end+1);
        change=edit(0,start,0,end,""); change.text=letters; change.length=inserted;
        value=xlsTextApply(base,&change,1,4095,&error);
        CHECK(value && !strcmp(xlsTextData(base),reference) && !strcmp(xlsTextData(value),next));
        CHECK(xlsTextSize(value)==strlen(next) && xlsTextLines(value)==1);
        xlsTextRelease(base); base=value; strcpy(reference,next);
    }
    xlsTextRelease(base);
    printf("analysis_text_test passed: %d checks; immutable XRT-backed snapshots, ordered atomic UTF-16 edits, Unicode/CRLF/BOM, bounds and 500 differential revisions.\n",checks);
    return 0;
}
