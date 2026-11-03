#include "src/sem/xl6_completion.h"
#include "src/common/xl6_output.h"
#include "src/common/xl6_path_identity.h"
#ifdef _WIN32
#include <windows.h>
#endif
int xl6CompletionTarget(XL6_CompletionQuery* q,const char* path)
{
 char* absolute;int equal;
 if (!q||!path) return 0;
 absolute=xrtPathAbs(path);
 if (!absolute) {q->error="cannot resolve completion path";return 0;}
 equal=xl6PathIdentityEqual(q->path,absolute);xrtFree(absolute);
 if (equal<0) {q->error="cannot compare completion path";return 0;}
 return equal;
}
int xl6CompletionAdd(XL6_CompletionQuery* q,const char* name,const char* kind,XL6_SymbolId symbolId,const XL6_AstNode* selection,char* canonical)
{
 XL6_CompletionItem *item,*items;size_t length,bytes,capacity;
 if (!name||!*name||!q||q->error) {xrtFree(canonical);return 0;}
 /* Resolver-private and qualified names require the import alias contract. */
 if (strchr(name,'.')||strncmp(name,"__xl6_",6)==0||(q->prefix&&strncmp(name,q->prefix,strlen(q->prefix)))) {xrtFree(canonical);return 1;}
 length=strlen(name);bytes=length+1+(canonical?strlen(canonical)+1:0);
 if (length>512||q->count>=XL6_COMPLETION_ITEMS||bytes>XL6_COMPLETION_BYTES-q->bytes) {q->error="completion exceeds item or text limit";xrtFree(canonical);return 0;}
 if (q->count==q->capacity) {
  capacity=q->capacity?q->capacity*2:64;items=(XL6_CompletionItem*)xrtRealloc(q->items,capacity*sizeof(*items));
  if (!items) {q->error="cannot allocate completion items";xrtFree(canonical);return 0;}
  q->items=items;q->capacity=capacity;
 }
 item=&q->items[q->count];memset(item,0,sizeof(*item));item->name=xrtStrDupN(name,length);
 if (!item->name) {q->error="cannot copy completion name";xrtFree(canonical);return 0;}
 item->canonical=canonical;item->kind=kind;item->symbolId=symbolId;item->selection=selection;q->count++;q->bytes+=bytes;return 1;
}
void xl6CompletionFree(XL6_CompletionQuery* q)
{
 size_t i;if (!q) return;
 for (i=0;i<q->count;++i) {xrtFree(q->items[i].name);xrtFree(q->items[i].canonical);}
 xrtFree(q->items);xrtFree(q->prefix);memset(q,0,sizeof(*q));
}
static int xl6CompletionSourceId(const XL6_SourceReport* r,const char* path)
{
 char* absolute;size_t i;int id=0;
 if (!path) return 0;
 absolute=xrtPathAbs(path);
 if (!absolute) return -1;
 for (i=0;i<r->count;++i) {
  if (!(r->items[i].kind&XL6_SOURCE_REPORT_SOURCE)) continue;
  int equal=xl6PathIdentityEqual(absolute,r->items[i].path);
  if (equal<0) {id=-1;break;}
  if (equal) {id=(int)i+1;break;}
 }
 xrtFree(absolute);return id;
}
static void xl6CompletionString(XL6_Output* out, const char* s)
{
    const unsigned char* p = (const unsigned char*)s;
    if (!s) { xl6OutputPuts("null", out); return; }
    xl6OutputPutc('"', out);
    for (; *p; ++p) {
        if (*p == '"' || *p == '\\') { xl6OutputPutc('\\', out); xl6OutputPutc(*p, out); }
        else if (*p < 32) xl6OutputPrintf(out, "\\u%04x", (unsigned)*p);
        else xl6OutputPutc(*p, out);
    }
    xl6OutputPutc('"', out);
}

static void xl6CompletionRange(XL6_Output* out, const XL6_SourceReport* report,
    unsigned source, const XL6_AstRange* r)
{
    const XL6_SourceReportItem* file;
    const unsigned char* text;
    size_t size, bom;
    if (!source || source > report->count || !r || !r->known) { xl6OutputPuts("null", out); return; }
    file = &report->items[source - 1];
    text = (const unsigned char*)file->text;
    bom = file->size >= 3 && text[0] == 0xef && text[1] == 0xbb && text[2] == 0xbf ? 3 : 0;
    text += bom; size = file->size - bom;
    if (r->endOffset < r->startOffset || r->endOffset > size ||
        (r->startOffset < size && (text[r->startOffset] & 0xc0) == 0x80) ||
        (r->endOffset < size && (text[r->endOffset] & 0xc0) == 0x80)) {
        xl6OutputPuts("null", out);
        return;
    }
    xl6OutputPrintf(out, "{\"startOffset\":%llu,\"endOffset\":%llu,\"startLine\":%d,\"startColumn\":%d,\"endLine\":%d,\"endColumn\":%d}",
        (unsigned long long)r->startOffset, (unsigned long long)r->endOffset,
        r->startLine, r->startColumn, r->endLine, r->endColumn);
}


char* xl6CompletionJson(XL6_CompletionQuery* q,const XL6_SourceReport* r)
{
 XL6_Output out;char* json=NULL;size_t i,size;int resolved=xl6CompletionSourceId(r,q->path);
 unsigned source=resolved>0?(unsigned)resolved:0;
 if (resolved<0) q->error="cannot resolve completion source identity";
 q->available=q->available&&!q->error&&q->parsed==1&&q->reached==1&&r->complete&&source;
 /* Resolve all identities first. A late lookup error must not publish an
  * already-started successful or partially mapped completion list. */
 for (i=0;q->available&&i<q->count;++i) {
  resolved=xl6CompletionSourceId(r,xl6AstSourcePath(q->items[i].selection));
  if (resolved<0) {q->error="cannot resolve completion selection identity";q->available=0;break;}
  q->items[i].selectionSourceId=(unsigned)resolved;
 }
 if (!xl6OutputOpenMemory(&out)) return NULL;
 xl6OutputPrintf(&out,"{\"schema\":1,\"scope\":\"cursor-query\",\"complete\":%s,\"checkedCompilation\":false,\"offsetEncoding\":\"utf-8-after-bom\",\"columnEncoding\":\"unicode-scalar-1-based\",\"rangeConvention\":\"half-open\",\"sourceId\":%u,\"offset\":%llu,\"replacementRange\":",q->available?"true":"false",source,(unsigned long long)q->offset);
 xl6CompletionRange(&out,r,source,&q->replacement);
 xl6OutputPuts(",\"prefix\":",&out);xl6CompletionString(&out,q->prefix);
 xl6OutputPuts(",\"error\":",&out);xl6CompletionString(&out,q->available?NULL:q->error?q->error:"cursor query is unavailable");xl6OutputPuts(",\"items\":[",&out);
 for (i=0;q->available&&i<q->count;++i) {
  const XL6_CompletionItem* item=&q->items[i];unsigned id=item->selectionSourceId;
  if (i) xl6OutputPutc(',',&out);
  xl6OutputPuts("{\"name\":",&out);xl6CompletionString(&out,item->name);xl6OutputPuts(",\"kind\":",&out);xl6CompletionString(&out,item->kind);
  xl6OutputPuts(",\"canonicalType\":",&out);xl6CompletionString(&out,item->canonical);
  xl6OutputPrintf(&out,",\"symbolId\":%u,\"selectionSourceId\":%u,\"selectionRange\":",(unsigned)item->symbolId,id);
  xl6CompletionRange(&out,r,id,item->selection?&item->selection->tokenRange:NULL);xl6OutputPutc('}',&out);
 }
 xl6OutputPuts("]}",&out);if (!xl6OutputHasFailed(&out)) json=xl6OutputTakeMemoryText(&out,&size);xl6OutputDiscard(&out);
 if (json&&size>8u*1024u*1024u) {xrtFree(json);json=NULL;}return json;
}
