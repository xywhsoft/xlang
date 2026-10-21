#include "analysis/text.h"
#include <xrt.h>
#include <string.h>

struct xls_text { xatomic32 references; xstrbuf bytes; xarray lines; };

static void setError(enum xls_text_error* error, enum xls_text_error value)
{ if(error) *error=value; }

const char* xlsTextError(enum xls_text_error error)
{
    static const char* messages[]={"OK", "Invalid text request", "Invalid UTF-8 source",
        "Invalid UTF-16 edit range", "Text capacity exceeded", "Cannot allocate text snapshot"};
    return error>=XLS_TEXT_OK && error<=XLS_TEXT_MEMORY ? messages[error] : messages[XLS_TEXT_ARGUMENT];
}

const char* xlsTextData(const xls_text* text)
{ return text && text->bytes.Data ? text->bytes.Data : ""; }
size_t xlsTextSize(const xls_text* text) { return text ? text->bytes.Size : 0; }
size_t xlsTextLines(const xls_text* text) { return text ? text->lines.Count : 0; }

static xls_text* allocateText(void)
{
    xls_text* text=xrtMalloc(sizeof(*text));
    if(!text) return NULL;
    memset(text,0,sizeof(*text));
    xrtAtomic32Init(&text->references,1);
    xrtStrBufInit(&text->bytes);
    if(!xrtArrayInit(&text->lines,sizeof(size_t))) { xrtFree(text); return NULL; }
    return text;
}

xls_text* xlsTextRetain(xls_text* text)
{
    if(text) xrtAtomic32FetchAdd(&text->references,1,XMEMORY_RELAXED);
    return text;
}

void xlsTextRelease(xls_text* text)
{
    if(text && xrtAtomic32FetchSub(&text->references,1,XMEMORY_ACQ_REL)==1) {
        xrtArrayUnit(&text->lines);
        xrtStrBufFree(&text->bytes);
        xrtFree(text);
    }
}

static int valid(const char* text, size_t length)
{ return (!length || text) && (!length || !memchr(text,0,length)) && xrtUtf8Valid((xstrview){text ? text : "",length},NULL); }

static int indexLines(xls_text* text)
{
    size_t at=0, start=0;
    const char* bytes=xlsTextData(text);
    if(!xrtArrayPush(&text->lines,&start)) return 0;
    while(at<text->bytes.Size) {
        char c=bytes[at++];
        if(c!='\r' && c!='\n') continue;
        if(c=='\r' && at<text->bytes.Size && bytes[at]=='\n') ++at;
        if(!xrtArrayPush(&text->lines,&at)) return 0;
    }
    return 1;
}

xls_text* xlsTextCreate(const char* text, size_t length, size_t limit, enum xls_text_error* error)
{
    setError(error,XLS_TEXT_OK);
    if(length>limit || length==(size_t)-1) { setError(error,XLS_TEXT_LIMIT); return NULL; }
    if(!valid(text,length)) { setError(error,XLS_TEXT_ENCODING); return NULL; }
    xls_text* result=allocateText();
    if(!result || !xrtStrBufAppend(&result->bytes,(xstrview){text ? text : "",length}) || !indexLines(result)) {
        xlsTextRelease(result); setError(error,XLS_TEXT_MEMORY); return NULL;
    }
    return result;
}

static int columnOffset(const char* bytes, size_t size, size_t start, size_t column, size_t* offset)
{
    size_t at=start, units=0;
    while(at<size && bytes[at]!='\r' && bytes[at]!='\n' && units<column) {
        uint32 scalar=0; size_t read=0;
        if(xrtUtf8Decode((xstrview){bytes+at,size-at},&scalar,&read)!=XUTF_OK) return 0;
        size_t width=scalar>0xffff ? 2u : 1u;
        if(width>column-units) return 0;
        units+=width; at+=read;
    }
    *offset=at; return 1;
}

int xlsTextOffset(const xls_text* text, xls_position position, size_t* offset)
{
    if(!text || !offset || position.line>=text->lines.Count) return 0;
    const size_t* start=xrtArrayConstGet(&text->lines,position.line);
    return columnOffset(xlsTextData(text),text->bytes.Size,*start,position.character,offset);
}

int xlsTextPosition(const xls_text* text, size_t offset, xls_position* position)
{
    if(!text || !position || offset>text->bytes.Size) return 0;
    size_t first=0,last=text->lines.Count;
    while(first<last) {
        size_t middle=first+(last-first)/2;
        const size_t* start=xrtArrayConstGet(&text->lines,middle);
        if(*start<=offset) first=middle+1; else last=middle;
    }
    size_t line=first-1,at=*(const size_t*)xrtArrayConstGet(&text->lines,line),units=0;
    const char* bytes=xlsTextData(text);
    while(at<offset) {
        if(bytes[at]=='\r' || bytes[at]=='\n') return 0;
        uint32 scalar=0; size_t read=0;
        if(xrtUtf8Decode((xstrview){bytes+at,text->bytes.Size-at},&scalar,&read)!=XUTF_OK || read>offset-at) return 0;
        units+=scalar>0xffff ? 2u : 1u; at+=read;
    }
    *position=(xls_position){line,units}; return 1;
}

static int boundary(const char* bytes, size_t size, size_t offset)
{
    return offset==size || (!((unsigned char)bytes[offset]>=0x80 && (unsigned char)bytes[offset]<0xc0) &&
        (!offset || bytes[offset-1]!='\r' || bytes[offset]!='\n'));
}

int xlsTextDiff(const xls_text* base, const xls_text* updated, xls_text_edit* edit)
{
    if(!base || !updated || !edit) return -1;
    memset(edit,0,sizeof(*edit));
    const char *left=xlsTextData(base),*right=xlsTextData(updated);
    size_t before=xlsTextSize(base),after=xlsTextSize(updated),prefix=0,suffix=0;
    while(prefix<before && prefix<after && left[prefix]==right[prefix]) ++prefix;
    if(prefix==before && prefix==after) return 0;
    while(prefix && (!boundary(left,before,prefix) || !boundary(right,after,prefix))) --prefix;
    while(suffix<before-prefix && suffix<after-prefix && left[before-suffix-1]==right[after-suffix-1]) ++suffix;
    while(suffix && (!boundary(left,before,before-suffix) || !boundary(right,after,after-suffix))) --suffix;
    if(!xlsTextPosition(base,prefix,&edit->start) || !xlsTextPosition(base,before-suffix,&edit->end)) return -1;
    edit->hasRange=1; edit->text=right+prefix; edit->length=after-prefix-suffix;
    return 1;
}

static int bufferOffset(const xstrbuf* buffer, xls_position position, size_t* offset)
{
    size_t at=0,line=0;
    const char* bytes=buffer->Data ? buffer->Data : "";
    while(at<buffer->Size && line<position.line) {
        char c=bytes[at++];
        if(c=='\r' || c=='\n') {
            if(c=='\r' && at<buffer->Size && bytes[at]=='\n') ++at;
            ++line;
        }
    }
    return line==position.line && columnOffset(bytes,buffer->Size,at,position.character,offset);
}

static size_t utf16Length(const char* bytes, size_t length)
{
    size_t at=0,units=0;
    while(at<length) {
        uint32 scalar=0; size_t read=0;
        if(xrtUtf8Decode((xstrview){bytes+at,length-at},&scalar,&read)!=XUTF_OK) return (size_t)-1;
        units+=scalar>0xffff ? 2u : 1u; at+=read;
    }
    return units;
}

xls_text* xlsTextApply(const xls_text* base, const xls_text_edit* edits, size_t count,
                     size_t limit, enum xls_text_error* error)
{
    setError(error,XLS_TEXT_OK);
    if(!base || (count && !edits)) { setError(error,XLS_TEXT_ARGUMENT); return NULL; }
    if(count>1024 || base->bytes.Size>limit || limit==(size_t)-1) { setError(error,XLS_TEXT_LIMIT); return NULL; }
    if(!count) return xlsTextRetain((xls_text*)base);
    xls_text* result=allocateText();
    if(!result || !xrtStrBufAppend(&result->bytes,xrtStrBufView(&base->bytes))) goto memory;
    for(size_t i=0;i<count;++i) {
        const xls_text_edit* edit=&edits[i];
        size_t start=0,end=result->bytes.Size;
        if(edit->length>limit) { setError(error,XLS_TEXT_LIMIT); goto failed; }
        if(!valid(edit->text,edit->length)) { setError(error,XLS_TEXT_ENCODING); goto failed; }
        if(edit->hasRange) {
            if(edit->start.line>edit->end.line || (edit->start.line==edit->end.line &&
                edit->start.character>edit->end.character)) { setError(error,XLS_TEXT_RANGE); goto failed; }
            int okay=i ? bufferOffset(&result->bytes,edit->start,&start) && bufferOffset(&result->bytes,edit->end,&end) :
                xlsTextOffset(base,edit->start,&start) && xlsTextOffset(base,edit->end,&end);
            if(!okay || end<start || (edit->hasRangeLength &&
                utf16Length(xlsTextData(result)+start,end-start)!=edit->rangeLength)) {
                setError(error,XLS_TEXT_RANGE); goto failed;
            }
        } else if(edit->hasRangeLength) { setError(error,XLS_TEXT_ARGUMENT); goto failed; }
        size_t retained=result->bytes.Size-(end-start);
        if(edit->length>limit-retained) { setError(error,XLS_TEXT_LIMIT); goto failed; }
        size_t next=retained+edit->length;
        size_t previous=result->bytes.Size;
        if(!xrtStrBufReserve(&result->bytes,next+1)) goto memory;
        if(next>previous && !xrtStrBufResize(&result->bytes,next)) goto memory;
        memmove(result->bytes.Data+start+edit->length,result->bytes.Data+end,previous-end+1);
        if(edit->length) memcpy(result->bytes.Data+start,edit->text,edit->length);
        if(next<=previous && !xrtStrBufResize(&result->bytes,next)) goto memory;
    }
    if(!indexLines(result)) goto memory;
    return result;
memory:
    setError(error,XLS_TEXT_MEMORY);
failed:
    xlsTextRelease(result); return NULL;
}
