#include "analysis/selection.h"
#include "analysis/frontend_internal.h"
#include <stdlib.h>
#include <string.h>

#define SELECTION_BYTES (8u*1024u*1024u)
typedef struct candidate { size_t position,start,end;xls_position first,last; } candidate;
typedef struct group { size_t first,count; } group;
typedef struct point { size_t offset,index; } point;
struct xls_selection { xarray ranges;group* groups;size_t count; };
typedef struct selection_visit {
    xls_selection* result;point* points;size_t count,size,prefix;
    int (*cancelled)(void*);void* user;size_t work;
} selection_visit;
static int pointCompare(const void* a,const void* b)
{
    const point *x=a,*y=b;
    if(x->offset!=y->offset)return x->offset<y->offset ? -1 : 1;
    return x->index<y->index ? -1 : x->index!=y->index;
}
static int rangeCompare(const void* a,const void* b)
{
    const candidate *x=a,*y=b;
    if(x->position!=y->position)return x->position<y->position ? -1 : 1;
    size_t xs=x->end-x->start,ys=y->end-y->start;
    if(xs!=ys)return xs<ys ? -1 : 1;
    return x->start<y->start ? -1 : x->start!=y->start;
}
static int append(xls_selection* result,candidate value)
{
    if(result->ranges.Count>=SELECTION_BYTES/(2*sizeof(candidate)) ||
       !xrtArrayPush(&result->ranges,&value))return 0;
    return result->ranges.Capacity<=SELECTION_BYTES/sizeof(candidate);
}
static int collectRange(selection_visit* context,size_t start,size_t end)
{
    if(start>=end || end>context->size)return 1;
    size_t low=0,high=context->count;
    while(low<high){size_t mid=low+(high-low)/2;if(context->points[mid].offset<start)low=mid+1;else high=mid;}
    for(size_t i=low;i<context->count && context->points[i].offset<=end;++i){
        if((context->work++&63u)==0 && context->cancelled && context->cancelled(context->user))return 0;
        if(!append(context->result,(candidate){context->points[i].index,start,end,{0},{0}}))return 0;
    }
    return 1;
}
static int collect(const XL6_AstNode* node,const XL6_AstRange* range,void* user)
{
    selection_visit* context=user;
    if(!range->known || range->startOffset>range->endOffset ||
       range->endOffset>context->size-context->prefix)return 1;
    size_t start=range->startOffset+context->prefix,end=range->endOffset+context->prefix;
    if(!collectRange(context,start,end))return -1;
    /* The visitor maps immutable incremental units into current coordinates.
     * A token inside its original node translates by the same byte delta. */
    XL6_AstRange token=node->tokenRange,original=node->range;
    if(token.known && original.known && token.startOffset>=original.startOffset &&
       token.endOffset>=token.startOffset && token.endOffset<=original.endOffset &&
       token.endOffset-original.startOffset<=end-start &&
       !collectRange(context,start+token.startOffset-original.startOffset,
                     start+token.endOffset-original.startOffset))return -1;
    return 1;
}
xls_selection* xlsSelectionCreate(const xls_syntax* syntax,const size_t* offsets,
    size_t count,int (*cancelled)(void*),void* user)
{
    if(!syntax || count>XLS_SELECTION_POSITIONS || (count && !offsets) ||
       (cancelled && cancelled(user)))return NULL;
    const xls_text* text=xlsSyntaxText(syntax);size_t size=xlsTextSize(text);
    point points[XLS_SELECTION_POSITIONS];
    for(size_t i=0;i<count;++i){xls_position p;if(!xlsTextPosition(text,offsets[i],&p))return NULL;points[i]=(point){offsets[i],i};}
    xls_selection* result=xrtCalloc(1,sizeof(*result));if(!result)return NULL;
    result->groups=count ? xrtCalloc(count,sizeof(*result->groups)) : NULL;result->count=count;
    if((count && !result->groups) || !xrtArrayInit(&result->ranges,sizeof(candidate)))goto failed;
    for(size_t i=0;i<count;++i){
        if(!xlsSyntaxAccepted(syntax) && !append(result,(candidate){i,offsets[i],offsets[i],{0},{0}}))goto failed;
        if(!append(result,(candidate){i,0,size,{0},{0}}))goto failed;
    }
    if(count && xlsSyntaxAccepted(syntax)){
        qsort(points,count,sizeof(*points),pointCompare);
        const unsigned char* bytes=(const unsigned char*)xlsTextData(text);
        size_t prefix=size>=3 && bytes[0]==0xef && bytes[1]==0xbb && bytes[2]==0xbf ? 3 : 0;
        selection_visit context={result,points,count,size,prefix,cancelled,user,0};
        if(!xlsSyntaxVisit(syntax,collect,&context,cancelled,user))goto failed;
    }
    if(cancelled && cancelled(user))goto failed;
    if(result->ranges.Count>1)qsort(result->ranges.Data,result->ranges.Count,sizeof(candidate),rangeCompare);
    candidate* ranges=(candidate*)result->ranges.Data;size_t written=0;
    for(size_t at=0;at<result->ranges.Count;++at){
        if((at&63u)==0 && cancelled && cancelled(user))goto failed;
        candidate item=ranges[at];group* g=&result->groups[item.position];
        if(g->count){candidate last=ranges[written-1];
            if(item.start>last.start || item.end<last.end || (item.start==last.start && item.end==last.end))continue;
        }else g->first=written;
        if(g->count==XLS_SELECTION_DEPTH)goto failed;
        if(!xlsTextPosition(text,item.start,&item.first) || !xlsTextPosition(text,item.end,&item.last))goto failed;
        ranges[written++]=item;++g->count;
    }
    result->ranges.Count=written;return result;
failed:xlsSelectionDestroy(result);return NULL;
}
void xlsSelectionDestroy(xls_selection* result)
{if(result){xrtArrayUnit(&result->ranges);xrtFree(result->groups);xrtFree(result);}}
size_t xlsSelectionCount(const xls_selection* result){return result ? result->count : 0;}
size_t xlsSelectionDepth(const xls_selection* result,size_t position)
{return result && position<result->count ? result->groups[position].count : 0;}
int xlsSelectionGet(const xls_selection* result,size_t position,size_t level,xls_selection_range* range)
{
    if(!range)return 0;
    *range=(xls_selection_range){0};
    if(!result || position>=result->count || level>=result->groups[position].count)return 0;
    const candidate* item=xrtArrayConstGet(&result->ranges,result->groups[position].first+level);
    if(!item)return 0;
    *range=(xls_selection_range){item->start,item->end,item->first,item->last};return 1;
}
