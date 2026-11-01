#include "analysis/frontend_type_arena.h"
int xlsFrontendTypeCanonicalAppend(const XL6_AstNode* root,const XL6_AstNode* type,
    size_t bytes,size_t depth,size_t visits,xstrbuf* arena,size_t limit,size_t* offset,size_t* length)
{
    if(offset)*offset=0;
    if(length)*length=0;
    if(!arena || !offset || !length || !bytes || bytes==SIZE_MAX || !depth || !visits || arena->Size>limit)return 0;
    size_t start=arena->Size;XL6_TypeStringBuilder builder;xl6TypeStringBuilderInit(&builder);
    builder.maxBytes=bytes;builder.maxDepth=depth;builder.visitsLeft=visits;builder.xlsArena=arena;builder.xlsArenaLimit=limit;
    xl6TypeAppendCanonicalBody(&builder,root,type);
    int okay=builder.ok && arena->Size<limit &&
        (arena->Size+1<=arena->Capacity || xrtStrBufReserve(arena,arena->Size+1)) && arena->Capacity<=limit;
    if(okay){arena->Data[arena->Size++]=0;arena->Data[arena->Size]=0;}
    if(okay){*offset=start;*length=builder.len;}
    else{arena->Size=start;if(arena->Data)arena->Data[start]=0;}
    xl6TypeStringBuilderFree(&builder);return okay;
}
