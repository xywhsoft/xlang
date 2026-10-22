#include "analysis/ast_clone.h"
#include <xrt.h>
#include <string.h>

typedef struct clone_context { xls_ast_clone_options options; xmap sources; size_t bytes; } clone_context;
static int charge(clone_context* c,size_t bytes)
{
    if(bytes>SIZE_MAX-c->bytes || (c->options.bytes && bytes>c->options.bytes-c->bytes)) return 0;
    c->bytes+=bytes; return 1;
}
static char* duplicate(clone_context* c,const char* text)
{ return text && charge(c,strlen(text)+1) ? xrtStrDup(text) : NULL; }
static int source(clone_context* c,const XL6_AstSource* in,XL6_AstNode* out)
{
    if(!in || c->options.dropSources) return 1;
    xbytesview key={(const unsigned char*)&in,sizeof(in)};
    XL6_AstSource** found=xrtMapGet(&c->sources,key); XL6_AstSource* value=found ? *found : NULL;
    if(!value) {
        if(!charge(c,sizeof(*value))) return 0;
        value=xrtCalloc(1,sizeof(*value)); if(!value) return 0;
        *value=*in; value->references=0; value->path=duplicate(c,in->path);
        if((in->path && !value->path) || !xrtMapSet(&c->sources,key,&value)) { xrtFree(value->path); xrtFree(value); return 0; }
    }
    out->source=value; ++value->references; return 1;
}
static XL6_AstNode* clone(clone_context* c,const XL6_AstNode* in,size_t depth)
{
    if(!in || depth>512 || !charge(c,sizeof(*in))) return NULL;
    XL6_AstNode* out=xrtCalloc(1,sizeof(*out)); if(!out) return NULL;
    *out=*in; out->text=out->doc=NULL; out->literalData=NULL; out->source=NULL;
    out->children=NULL; out->childCount=out->childCapacity=0; out->inferredType=out->semanticDeclaredType=NULL;
    if(c->options.transform && !c->options.transform(in,out,c->options.user)) goto failed;
    if(in->text && !(out->text=duplicate(c,in->text))) goto failed;
    if(in->doc && !(out->doc=duplicate(c,in->doc))) goto failed;
    if(!source(c,in->source,out)) goto failed;
    if(in->literalData) {
        if(in->literalSize==SIZE_MAX || !charge(c,in->literalSize+1) || !xl6AstSetLiteralData(out,in->literalData,in->literalSize)) goto failed;
        out->literalDecoded=in->literalDecoded;
    }
    if(in->childCount) {
        if(in->childCount>SIZE_MAX/sizeof(*out->children) || !charge(c,in->childCount*sizeof(*out->children))) goto failed;
        out->children=xrtCalloc(in->childCount,sizeof(*out->children)); if(!out->children) goto failed;
        out->childCapacity=in->childCount;
        for(size_t i=0;i<in->childCount;++i) {
            XL6_AstNode* child=clone(c,in->children[i],depth+1); if(!child) goto failed;
            out->children[out->childCount++]=child;
        }
    }
    if(in->inferredType && !(out->inferredType=clone(c,in->inferredType,depth+1))) goto failed;
    if(in->semanticDeclaredType && !(out->semanticDeclaredType=clone(c,in->semanticDeclaredType,depth+1))) goto failed;
    return out;
failed: xl6AstFree(out); return NULL;
}
XL6_AstNode* xlsAstClone(const XL6_AstNode* input,const xls_ast_clone_options* options,size_t* bytes)
{
    if(bytes) *bytes=0;
    clone_context context={0}; if(options) context.options=*options;
    if(!input || !xrtMapInit(&context.sources,sizeof(XL6_AstSource*))) return NULL;
    XL6_AstNode* root=clone(&context,input,0); xrtMapUnit(&context.sources);
    if(root && bytes) *bytes=context.bytes;
    return root;
}
