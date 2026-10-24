/* Appended to the audited shared parser TU: reuse its exact type matcher. */
#include "analysis/call_context.h"
#include "src/lexer/xl6_literal_span.h"
#define XLS_CALL_NONE SIZE_MAX
#define XLS_CALL_DEPTH 256u
#define XLS_CALL_TYPE_TOKENS 512u
/* Keep no 128-byte lexer diagnostic buffer per retained scratch token. The
 * original virtual matcher reads only kind/text/len from its borrowed window. */
typedef struct xls_call_token {XL6_TokenKind kind;const char* text;size_t len,startOffset,endOffset;} xls_call_token;
typedef struct xls_call_tokens {xls_call_token* tokens;size_t count,capacity;} xls_call_tokens;
typedef struct xls_call_state {size_t call,argument,namedStart,namedEnd;int known,embedded;size_t begin;} xls_call_state;
typedef struct xls_call_point {
    size_t start,end,commentStart,commentEnd;
    xls_call_state state;
    int literal,embedded,error;
} xls_call_point;
typedef struct xls_call_frame {
    size_t start,end,nameStart,nameEnd,open,close;
    int closed;
} xls_call_frame;
typedef struct xls_call_expr {size_t start,nameStart,nameEnd;int tail;} xls_call_expr;
typedef struct xls_call_level {
    XL6_TokenKind kind;size_t open,first,argument,namedStart,namedEnd,call;
    int known,embedded;
    size_t begin;
} xls_call_level;
struct xls_call_context {
    xatomic32 references;xls_text* text;size_t bom,size,count,frameCount;
    xls_call_point* points;xls_call_frame* frames;xls_call_context_work work;
};
static int xlsCallWhitespace(unsigned char c)
{return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\v' || c=='\f';}
static int xlsCallCharge(size_t* total,size_t bytes,size_t limit)
{if(*total>limit || bytes>limit-*total)return 0;*total+=bytes;return 1;}
static xls_call_state xlsCallState(const xls_call_level* stack,size_t depth)
{
    while(depth){const xls_call_level* p=&stack[--depth];if(p->call!=XLS_CALL_NONE)return (xls_call_state){p->call,p->argument,p->namedStart,p->namedEnd,p->known,p->embedded,p->begin};}
    return (xls_call_state){.call=XLS_CALL_NONE};
}
static int xlsCallContainer(const xls_call_token* token)
{
    XL6_Token original={.kind=token->kind,.text=token->text,.len=token->len};
    return xl6ParserIsContainerLiteralTypeName(&original);
}
static int xlsCallGeneric(const xls_call_tokens* stream,size_t at,size_t* end)
{
    /* Bound the window at the first structural closing angle,then let the
     * unchanged parser prove its contents (including virtual nested >>).
     * A failed proof never certifies the apparent generic commas. */
    XL6_Token buffer[XLS_CALL_TYPE_TOKENS];size_t count=0;int depth=0;
    for(;count<XLS_CALL_TYPE_TOKENS && at+count<stream->count;++count){
        const xls_call_token* in=&stream->tokens[at+count];
        buffer[count].kind=in->kind;buffer[count].text=in->text;buffer[count].len=in->len;
        if(in->kind==XL6_TK_LT)++depth;
        else if(in->kind==XL6_TK_GT)--depth;
        else if(in->kind==XL6_TK_SHR)depth-=2;
        if(depth<=0){++count;break;}
    }
    XL6_ParseTokenStream window={.tokens=buffer,.count=count};
    XL6_VTokenPos args={1,0},close,next;
    if(!xl6ParserVMatchTypeList(&window,args,&close) || xl6ParserVKindAt(&window,close)!=XL6_TK_GT)return 0;
    next=xl6ParserVNext(&window,close);if(next.secondGt)return 0;*end=at+next.index;return 1;
}
void xlsCallContextRelease(xls_call_context* context)
{
    if(!context || xrtAtomic32FetchSub(&context->references,1,XMEMORY_ACQ_REL)!=1)return;
    xlsTextRelease(context->text);xrtFree(context->points);xrtFree(context->frames);xrtFree(context);
}
xls_call_context* xlsCallContextRetain(xls_call_context* context)
{if(context)xrtAtomic32FetchAdd(&context->references,1,XMEMORY_RELAXED);return context;}
const xls_text* xlsCallContextText(const xls_call_context* context){return context ? context->text : NULL;}
xls_call_context_work xlsCallContextWork(const xls_call_context* context){return context ? context->work : (xls_call_context_work){0};}
xls_call_context* xlsCallContextCreate(xls_text* text,size_t limit,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_call_context* result=NULL;xls_call_tokens tokens={0};XL6_Lexer lexer={0};xls_call_expr* expressions=NULL;
    const size_t fixedScratch=XLS_CALL_DEPTH*sizeof(xls_call_level)+XLS_CALL_TYPE_TOKENS*sizeof(XL6_Token);
    const char* reason="call context allocation or capacity limit";size_t used=sizeof(xls_call_context)+fixedScratch,rawBytes=0;
    if(error && capacity)*error=0;
    if(limit>64u*1024u*1024u)limit=64u*1024u*1024u;
    if(!text || xlsTextSize(text)>8u*1024u*1024u || !xlsCallCharge(&used,xlsTextStorageBytes(text),limit))goto failed;
    const char* source=xlsTextData(text);size_t bom=xlsTextSize(text)>=3 && !memcmp(source,"\xef\xbb\xbf",3) ? 3 : 0;
    if(cancelled && cancelled(user))goto cancel;
    result=xrtCalloc(1,sizeof(*result));if(!result)goto failed;xrtAtomic32Init(&result->references,1);
    result->text=xlsTextRetain(text);result->bom=bom;result->size=xlsTextSize(text)-bom;source+=bom;
    if(!xl6LexerOpen(&lexer,source))goto failed;
    for(;;){
        if(cancelled && cancelled(user))goto cancel;
        if(tokens.count==tokens.capacity){
            size_t next=tokens.capacity ? tokens.capacity*2 : 128;
            if(next<tokens.capacity || next>SIZE_MAX/sizeof(xls_call_token) || !xlsCallCharge(&used,(next-tokens.capacity)*sizeof(xls_call_token),limit))goto failed;
            xls_call_token* p=xrtRealloc(tokens.tokens,next*sizeof(*p));if(!p)goto failed;tokens.tokens=p;tokens.capacity=next;
        }
        XL6_Token token=xl6LexerNext(&lexer);tokens.tokens[tokens.count++]=(xls_call_token){token.kind,token.text,token.len,token.startOffset,token.endOffset};
        result->work.lexedBytes=token.endOffset;
        if(token.kind==XL6_TK_ERROR){result->work.lexicalError=1;break;}
        if(token.kind==XL6_TK_EOF)break;
    }
    /* Token text may borrow the scanner. All text tests below use original
     * source offsets,except the original matcher whose token.text is live
     * until after construction. */
    rawBytes=tokens.capacity*sizeof(xls_call_token);
    if(tokens.count>SIZE_MAX/sizeof(xls_call_point) || !xlsCallCharge(&used,tokens.count*sizeof(xls_call_point),limit) ||
       tokens.count>SIZE_MAX/sizeof(xls_call_frame) || !xlsCallCharge(&used,tokens.count*sizeof(xls_call_frame),limit) ||
       tokens.count>SIZE_MAX/sizeof(xls_call_expr) || !xlsCallCharge(&used,tokens.count*sizeof(xls_call_expr),limit))goto failed;
    result->points=xrtCalloc(tokens.count,sizeof(*result->points));result->frames=xrtCalloc(tokens.count,sizeof(*result->frames));
    expressions=xrtCalloc(tokens.count,sizeof(*expressions));if(!result->points || !result->frames || !expressions)goto failed;
    xls_call_level stack[XLS_CALL_DEPTH];size_t depth=0,skip=0,previousEnd=0;result->count=tokens.count;
    for(size_t i=0;i<tokens.count;++i){
        if(cancelled && cancelled(user))goto cancel;
        xls_call_token* token=&tokens.tokens[i];xls_call_point* point=&result->points[i];point->start=token->startOffset;point->end=token->endOffset;
        point->commentStart=point->commentEnd=XLS_CALL_NONE;
        for(size_t j=previousEnd;j<point->start && j<result->size;++j)if(!xlsCallWhitespace((unsigned char)source[j])){if(point->commentStart==XLS_CALL_NONE)point->commentStart=j;point->commentEnd=j+1;}
        previousEnd=point->end;
        point->literal=token->kind==XL6_TK_STRING || token->kind==XL6_TK_BYTES || token->kind==XL6_TK_CHAR || token->kind==XL6_TK_FSTRING || token->kind==XL6_TK_DOC;
        point->embedded=token->kind==XL6_TK_FSTRING;point->error=token->kind==XL6_TK_ERROR;
        if(point->error){XL6_LiteralFrame literal;size_t content;if(xl6LiteralOpening(source,result->size,token->startOffset,&literal,&content)){point->literal=1;point->embedded=literal.interpolated!=0;}}
        if(i<skip){point->state=xlsCallState(stack,depth);continue;}
        int explicitType=token->kind==XL6_TK_LT && i && tokens.tokens[i-1].kind==XL6_TK_SCOPE;
        int containerType=token->kind==XL6_TK_LT && i && xlsCallContainer(&tokens.tokens[i-1]);
        if(explicitType || containerType){
            size_t end=0;int matched=xlsCallGeneric(&tokens,i,&end);
            if(matched && containerType)matched=end<tokens.count && (tokens.tokens[end].kind==XL6_TK_LBRACE || tokens.tokens[end].kind==XL6_TK_LBRACKET);
            if(matched){
                ++result->work.genericProofs;skip=end;
                if(explicitType && i>=2)expressions[end-1]=expressions[i-2];
                point->state=xlsCallState(stack,depth);continue;
            }
            ++result->work.genericDeclines;for(size_t j=depth;j>0;--j)if(stack[j-1].call!=XLS_CALL_NONE){stack[j-1].known=0;break;}
        }
        if(token->kind==XL6_TK_IDENT){
            expressions[i]=(xls_call_expr){token->startOffset,token->startOffset,token->endOffset,1};
            if(i>=2 && (tokens.tokens[i-1].kind==XL6_TK_DOT || tokens.tokens[i-1].kind==XL6_TK_SAFE_DOT) && expressions[i-2].tail)expressions[i].start=expressions[i-2].start;
        }
        if(token->kind==XL6_TK_LPAREN || token->kind==XL6_TK_LBRACKET || token->kind==XL6_TK_SAFE_LBRACKET || token->kind==XL6_TK_LBRACE){
            if(depth==XLS_CALL_DEPTH){reason="call context nesting limit";goto failed;}
            xls_call_level level={.kind=token->kind,.open=i,.first=i+1,.call=XLS_CALL_NONE,.known=1,.begin=token->endOffset};
            int declaration=i>=2 && (tokens.tokens[i-2].kind==XL6_KW_FUNC || tokens.tokens[i-2].kind==XL6_KW_GET || tokens.tokens[i-2].kind==XL6_KW_LET);
            if(token->kind==XL6_TK_LPAREN && i && expressions[i-1].tail && !declaration){
                xls_call_expr callee=expressions[i-1];level.call=result->frameCount++;
                result->frames[level.call]=(xls_call_frame){callee.start,tokens.tokens[i-1].endOffset,callee.nameStart,callee.nameEnd,token->endOffset,result->size,0};
            }
            stack[depth++]=level;
        }else if(token->kind==XL6_TK_RPAREN || token->kind==XL6_TK_RBRACKET || token->kind==XL6_TK_RBRACE){
            int matched=depth && ((token->kind==XL6_TK_RPAREN && stack[depth-1].kind==XL6_TK_LPAREN) ||
                (token->kind==XL6_TK_RBRACKET && (stack[depth-1].kind==XL6_TK_LBRACKET || stack[depth-1].kind==XL6_TK_SAFE_LBRACKET)) ||
                (token->kind==XL6_TK_RBRACE && stack[depth-1].kind==XL6_TK_LBRACE));
            if(!matched){depth=0;}else{
                xls_call_level level=stack[--depth];size_t start=tokens.tokens[level.open].startOffset;
                if(level.call!=XLS_CALL_NONE){xls_call_frame* frame=&result->frames[level.call];frame->closed=1;frame->close=token->startOffset;start=frame->start;}
                else if(token->kind==XL6_TK_RBRACKET && level.open && expressions[level.open-1].tail)start=expressions[level.open-1].start;
                expressions[i]=(xls_call_expr){.start=start,.tail=token->kind!=XL6_TK_RBRACE};
            }
        }else if(token->kind==XL6_TK_COMMA && depth && stack[depth-1].call!=XLS_CALL_NONE){
            xls_call_level* level=&stack[depth-1];++level->argument;level->namedStart=level->namedEnd=0;level->first=i+1;level->begin=token->endOffset;
        }else if(token->kind==XL6_TK_ASSIGN && depth && stack[depth-1].call!=XLS_CALL_NONE){
            xls_call_level* level=&stack[depth-1];if(i && level->first==i-1 && tokens.tokens[i-1].kind==XL6_TK_IDENT){level->namedStart=tokens.tokens[i-1].startOffset;level->namedEnd=tokens.tokens[i-1].endOffset;}
        }else if(token->kind==XL6_KW_FUNC && depth){
            /* Embedded anonymous bodies need the parser's own recovery before
             * semantic signature publication. Preserve a candidate but never
             * claim a proven outer argument number across that boundary. */
            for(size_t j=depth;j>0;--j)if(stack[j-1].call!=XLS_CALL_NONE){stack[j-1].known=0;stack[j-1].embedded=1;break;}
        }else if(token->kind==XL6_KW_END){
            int embedded=0;for(size_t j=0;j<depth;++j)embedded|=stack[j].embedded;
            if(!embedded)depth=0;
        }
        point->state=xlsCallState(stack,depth);
    }
    xl6LexerClose(&lexer);xrtFree(expressions);xrtFree(tokens.tokens);
    result->work.tokens=result->count;result->work.calls=result->frameCount;
    result->work.constructionBytes=used;
    result->work.chargedBytes=used-rawBytes-tokens.count*sizeof(xls_call_expr)-fixedScratch;
    return result;
cancel:reason="call context cancelled";
failed:
    xl6LexerClose(&lexer);xrtFree(expressions);xrtFree(tokens.tokens);xlsCallContextRelease(result);
    if(error && capacity)snprintf(error,capacity,"%s",reason);
    return NULL;
}
static int xlsCallLookup(const xls_call_context* context,size_t offset,xls_call_site* site,int recovering)
{
    if(site)*site=(xls_call_site){0};
    if(!context || !site || offset>context->size)return -1;
    const char* source=xlsTextData(context->text)+context->bom;
    if((offset<context->size && ((unsigned char)source[offset]&0xc0)==0x80) || (offset && offset<context->size && source[offset-1]=='\r' && source[offset]=='\n'))return -1;
    size_t lo=0,hi=context->count;
    while(lo<hi){size_t mid=lo+(hi-lo)/2;if(context->points[mid].end<=offset)lo=mid+1;else hi=mid;}
    const xls_call_point* next=lo<context->count ? &context->points[lo] : NULL;
    int sourceError=(next && next->error && offset>=next->start) || (!next && context->work.lexicalError);
    if(sourceError && !recovering)return 0;
    xls_call_state state=lo ? context->points[lo-1].state : (xls_call_state){.call=XLS_CALL_NONE};
    if(state.call==XLS_CALL_NONE || state.call>=context->frameCount)return 0;
    const xls_call_frame* frame=&context->frames[state.call];
    *site=(xls_call_site){frame->start,frame->end,frame->nameStart,frame->nameEnd,frame->open,frame->close,state.argument,state.namedStart,state.namedEnd,frame->closed,state.known,0,0,state.embedded,state.begin,sourceError};
    if(next){site->inComment=next->commentStart!=XLS_CALL_NONE && offset>=next->commentStart && offset<next->commentEnd;site->inLiteral=next->literal && offset>next->start;site->embedded|=site->inLiteral && next->embedded;}
    else if(sourceError && context->count){const xls_call_point* last=&context->points[context->count-1];site->inLiteral=last->literal && offset>last->start;site->embedded|=site->inLiteral && last->embedded;}
    return 1;
}
int xlsCallContextLookup(const xls_call_context* c,size_t offset,xls_call_site* site)
{return xlsCallLookup(c,offset,site,0);}
int xlsCallContextLookupRecovering(const xls_call_context* c,size_t offset,xls_call_site* site)
{return xlsCallLookup(c,offset,site,1);}
