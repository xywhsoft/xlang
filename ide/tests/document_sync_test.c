#include "protocol/document_sync.h"
#include "analysis/text.h"
#include <stdio.h>
#include <string.h>

static int checks;
#define CHECK(call) do { ++checks; if(!(call)) { fprintf(stderr,"document_sync_test:%d: %s\n",__LINE__,#call); return 1; } } while(0)
typedef struct received { const char* method; xvalue* params; } received;
typedef struct sink { xarray messages; size_t calls,failAt; } sink;
static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
static const xvalue* get(const xvalue* value,const char* name) { return xrtValueObjectGet(value,sv(name)); }
static int number(const xvalue* value) { int64 n=-1; xrtValueGetInt(value,&n); return (int)n; }
static int stringIs(const xvalue* value,const char* text)
{ xstrview view={0}; return xrtValueGetString(value,&view) && view.Size==strlen(text) && !memcmp(view.Data,text,view.Size); }
static int emit(void* user,const char* method,const xvalue* params)
{
    sink* target=user; ++target->calls;
    if(target->failAt && target->calls==target->failAt) return 0;
    received message={method,xrtValueRetain((xvalue*)params)};
    if(xrtArrayPush(&target->messages,&message)) return 1;
    xrtValueRelease(message.params); return 0;
}
static void clear(sink* target)
{
    for(size_t i=0;i<target->messages.Count;++i) xrtValueRelease(((received*)xrtArrayGet(&target->messages,i))->params);
    xrtArrayClear(&target->messages); target->calls=0; target->failAt=0;
}
static received* message(sink* target,size_t at) { return xrtArrayGet(&target->messages,at); }

int main(void)
{
    sink target={0}; CHECK(xrtArrayInit(&target.messages,sizeof(received)));
    xls_sync* sync=xlsSyncCreate(2); CHECK(sync && !xlsSyncCreate(0));
    const char *a="A😀Z\r\n",*b="dependency\n",*updated="A😁Z\r\n";
    xls_sync_document docs[]={{"file:///main.xl",a,strlen(a),1,1},{"file:///dep.xl",b,strlen(b),2,1}};
    CHECK(xlsSyncUpdate(sync,docs,2,emit,&target) && target.messages.Count==2);
    CHECK(!strcmp(message(&target,0)->method,"textDocument/didOpen") && xlsSyncVersion(sync,docs[0].uri)==1);
    CHECK(stringIs(get(get(message(&target,0)->params,"textDocument"),"text"),a));
    clear(&target);
    CHECK(xlsSyncUpdate(sync,docs,2,emit,&target) && !target.calls);
    docs[0].text=updated; docs[0].length=strlen(updated); ++docs[0].revision;
    CHECK(xlsSyncUpdate(sync,docs,2,emit,&target) && target.messages.Count==1 && xlsSyncVersion(sync,docs[0].uri)==2);
    received* delta=message(&target,0);
    CHECK(!strcmp(delta->method,"textDocument/didChange"));
    const xvalue* change=xrtValueArrayGet(get(delta->params,"contentChanges"),0);
    CHECK(stringIs(get(change,"text"),"😁") && !get(change,"rangeLength"));
    CHECK(number(get(get(get(change,"range"),"start"),"character"))==1);
    CHECK(number(get(get(get(change,"range"),"end"),"character"))==3);
    xls_sync_stats stats; xlsSyncStats(sync,&stats);
    CHECK(stats.opens==2 && stats.changes==1 && !stats.closes && stats.textBytes==strlen(a)+strlen(b)+4);
    clear(&target); ++docs[0].revision;
    CHECK(xlsSyncUpdate(sync,docs,2,emit,&target) && !target.calls && xlsSyncVersion(sync,docs[0].uri)==2);
    docs[0].identity=99;
    CHECK(xlsSyncUpdate(sync,docs,2,emit,&target) && target.messages.Count==2 && xlsSyncVersion(sync,docs[0].uri)==1);
    CHECK(!strcmp(message(&target,0)->method,"textDocument/didClose") && !strcmp(message(&target,1)->method,"textDocument/didOpen"));
    clear(&target);
    CHECK(xlsSyncUpdate(sync,docs,1,emit,&target) && target.messages.Count==1 && !xlsSyncVersion(sync,docs[1].uri));
    CHECK(stringIs(get(get(message(&target,0)->params,"textDocument"),"uri"),docs[1].uri));
    clear(&target);
    docs[1]=docs[0];
    CHECK(!xlsSyncUpdate(sync,docs,2,emit,&target) && !target.calls && xlsSyncVersion(sync,docs[0].uri)==1);
    docs[1]=(xls_sync_document){"file:///bad.xl","\xc0\x80",2,3,1};
    CHECK(!xlsSyncUpdate(sync,docs,2,emit,&target) && !target.calls && xlsSyncVersion(sync,docs[0].uri)==1);
    docs[1]=(xls_sync_document){"file:///bad.xl","x",8u*1024u*1024u+1,3,1};
    CHECK(!xlsSyncUpdate(sync,docs,2,emit,&target) && !target.calls);
    CHECK(!xlsSyncUpdate(sync,docs,65,emit,&target) && !target.calls);
    CHECK(xlsSyncUpdate(sync,NULL,0,emit,&target) && target.messages.Count==1 && !xlsSyncVersion(sync,docs[0].uri));
    clear(&target); xlsSyncDestroy(sync);

    sync=xlsSyncCreate(1); docs[0]=(xls_sync_document){"file:///main.xl",a,strlen(a),1,1};
    CHECK(sync && xlsSyncUpdate(sync,docs,1,emit,&target)); clear(&target);
    docs[0].text=updated; ++docs[0].revision;
    CHECK(xlsSyncUpdate(sync,docs,1,emit,&target) && target.messages.Count==1);
    change=xrtValueArrayGet(get(message(&target,0)->params,"contentChanges"),0);
    CHECK(!get(change,"range") && stringIs(get(change,"text"),updated));
    xlsSyncDestroy(sync); clear(&target);

    sync=xlsSyncCreate(2); docs[1]=(xls_sync_document){"file:///dep.xl",b,strlen(b),2,1};
    target.failAt=2;
    CHECK(!xlsSyncUpdate(sync,docs,2,emit,&target) && target.calls==2 && !xlsSyncVersion(sync,docs[0].uri));
    clear(&target);
    CHECK(!xlsSyncUpdate(sync,docs,2,emit,&target) && !target.calls);
    xlsSyncDestroy(sync);

    sync=xlsSyncCreate(2); char uris[64][48]; xls_sync_document many[64];
    for(size_t i=0;i<64;++i) {
        snprintf(uris[i],sizeof(uris[i]),"file:///document-%zu.xl",i);
        many[i]=(xls_sync_document){uris[i],"x",1,(uint64_t)i+1,1};
    }
    CHECK(sync && xlsSyncUpdate(sync,many,64,emit,&target) && target.messages.Count==64);
    clear(&target);
    CHECK(xlsSyncUpdate(sync,many,64,emit,&target) && !target.calls);
    CHECK(xlsSyncUpdate(sync,NULL,0,emit,&target) && target.messages.Count==64);
    clear(&target); xlsSyncDestroy(sync);

    const char* cases[]={"","a","a\r\nb","a\nb","a\rb","a😀b","a😁b","中\n尾","\xef\xbb\xbf中","a\r","a\r\n","abcdef"};
    for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i) for(size_t j=0;j<sizeof(cases)/sizeof(*cases);++j) {
        xls_text* old=xlsTextCreate(cases[i],strlen(cases[i]),4096,NULL);
        xls_text* next=xlsTextCreate(cases[j],strlen(cases[j]),4096,NULL);
        xls_text_edit edit={0}; int changed=xlsTextDiff(old,next,&edit);
        CHECK(old && next && changed==(strcmp(cases[i],cases[j])!=0));
        xls_text* applied=xlsTextApply(old,&edit,changed ? 1 : 0,4096,NULL);
        CHECK(applied && !strcmp(xlsTextData(applied),cases[j]) && !strcmp(xlsTextData(old),cases[i]));
        for(size_t at=0;at<=strlen(cases[i]);++at) {
            xls_position position={0}; size_t offset=999;
            if(xlsTextPosition(old,at,&position)) CHECK(xlsTextOffset(old,position,&offset) && offset==at);
        }
        xlsTextRelease(old); xlsTextRelease(next); xlsTextRelease(applied);
    }
    clear(&target); xrtArrayUnit(&target.messages);
    printf("document_sync_test passed: %d checks; retained revisions, delta/full modes, Unicode/CRLF alignment, atomic validation, document identity and failed transport isolation.\n",checks);
    return 0;
}
