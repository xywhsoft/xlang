/* Included after the original token finalizer,within the shared parser TU. */
#include "analysis/signature_internal.h"
#include "analysis/frontend_completion_path.h"
static _Thread_local xls_signature_scope* xlsActiveSignature;
void xlsSignatureScopeBegin(xls_signature_scope* scope,xls_signature_data* data,int (*cancelled)(void*),void* user)
{
    *scope=(xls_signature_scope){.previous=xlsActiveSignature,.data=data,.cancelled=cancelled,.cancelUser=user};xlsActiveSignature=scope;
}
void xlsSignatureScopeEnd(xls_signature_scope* scope)
{if(scope && xlsActiveSignature==scope)xlsActiveSignature=scope->previous;}
xls_signature_scope* xlsSignatureScopeCurrent(const XL6_CompletionQuery* query)
{return xlsActiveSignature && query && xlsActiveSignature->query==query && xlsActiveSignature->data->hasSite ? xlsActiveSignature : NULL;}
int xlsSignatureScopeCancelled(xls_signature_scope* scope)
{if(!scope)return 0;if(!scope->cancelObserved && scope->cancelled && scope->cancelled(scope->cancelUser))scope->cancelObserved=1;return scope->cancelObserved;}
static int xlsSignatureToken(XL6_ParseTokenStream* stream,XL6_Token token,XL6_CompletionQuery* query)
{
    if(xl6ParseTokenStreamAppend(stream,&token))return 1;
    query->error="cannot allocate signature projection tokens";return 0;
}
static XL6_Token xlsSignatureSynthetic(XL6_Lexer* lexer,XL6_TokenKind kind,const char* spelling)
{
    XL6_Token token={.kind=kind,.text=spelling,.len=strlen(spelling),.startOffset=lexer->extra.offset,.endOffset=lexer->extra.offset,
        .startLine=lexer->extra.line,.endLine=lexer->extra.line,.startColumn=lexer->extra.column,.endColumn=lexer->extra.column};return token;
}
static int xlsSignatureReadTokens(XL6_Lexer* lexer,XL6_ParseTokenStream* stream,XL6_DiagList* diags,const char* raw,XL6_CompletionQuery* query)
{
    xls_signature_scope* scope=xlsSignatureScopeCurrent(query);
    if(!scope)return xl6ParserReadTokens(lexer,stream,diags);
    xls_signature_data* data=scope->data;const char* source=raw;size_t size=strlen(raw);
    if(size>=3 && !memcmp(source,"\xef\xbb\xbf",3)){source+=3;size-=3;}
    const xls_call_site* site=&data->site;
    if(size!=data->size || memcmp(source,data->source,size) || !xlsCompletionPathEqual(data->path,query->path) || query->offset!=data->offset ||
       !site->argumentsStart || site->argumentsStart>size || source[site->argumentsStart-1]!='(' || site->argumentStart<site->argumentsStart || site->argumentStart>data->offset){query->error="signature projection does not match the original source";return 0;}
    XL6_TokenKind stack[256];size_t depth=0;int opened=0,projected=0;
    for(;;){
        if(xlsSignatureScopeCancelled(scope)){query->error="signature projection cancelled";return 0;}
        if(!projected && opened && lexer->extra.offset>=site->argumentStart){
            if(lexer->extra.offset!=site->argumentStart || !depth || stack[depth-1]!=XL6_TK_LPAREN){query->error="signature argument boundary is not proven";return 0;}
            XL6_Token dummy=xlsSignatureSynthetic(lexer,XL6_TK_INT,"0");
            if(!xlsSignatureToken(stream,dummy,query))return 0;
            size_t end=site->argumentsEnd;
            if(site->closed){if(end>=size || source[end]!=')'){query->error="signature closing parenthesis is not proven";return 0;}}
            else{end=data->offset;while(end<size && source[end]!='\r' && source[end]!='\n')++end;}
            int paren=lexer->extra.parenDepth,bracket=lexer->extra.bracketDepth,brace=lexer->extra.braceDepth;
            if(!xlsFrontendLexerResume(lexer,source,end,paren,bracket,brace)){query->error="cannot resume signature source scanner";return 0;}
            if(!site->closed){
                while(depth){XL6_TokenKind kind=stack[--depth],close=kind==XL6_TK_LPAREN ? XL6_TK_RPAREN : kind==XL6_TK_LBRACE ? XL6_TK_RBRACE : XL6_TK_RBRACKET;
                    const char* spelling=close==XL6_TK_RPAREN ? ")" : close==XL6_TK_RBRACE ? "}" : "]";
                    if(!xlsSignatureToken(stream,xlsSignatureSynthetic(lexer,close,spelling),query))return 0;
                }
                lexer->extra.parenDepth=lexer->extra.bracketDepth=lexer->extra.braceDepth=0;
            }
            projected=data->projected=1;
        }
        XL6_Token token=xl6LexerNext(lexer);
        if(token.kind==XL6_TK_EOF){token.text=source+size;token.len=0;token.startOffset=token.endOffset=size;token.startLine=token.endLine=lexer->extra.line;token.startColumn=token.endColumn=lexer->extra.column;}
        if(token.text && token.len){
            /* Every retained token must outlive restarting the Flex buffer.
             * Keep only exact original slices; synthesized tokens above have
             * their own static spelling and zero-width derived ranges. */
            if(token.endOffset<token.startOffset || token.endOffset>size || token.len!=token.endOffset-token.startOffset || memcmp(token.text,source+token.startOffset,token.len)){query->error="signature token is not an original source slice";return 0;}
            token.text=source+token.startOffset;
        }
        if(token.kind==XL6_TK_ERROR){query->error="signature prerequisites contain a lexical error";return 0;}
        if(token.kind==XL6_TK_LPAREN || token.kind==XL6_TK_LBRACKET || token.kind==XL6_TK_SAFE_LBRACKET || token.kind==XL6_TK_LBRACE){
            if(depth==256){query->error="signature projection nesting limit";return 0;}stack[depth++]=token.kind;
            if(token.kind==XL6_TK_LPAREN && token.startOffset==site->argumentsStart-1)opened=1;
        }else if(token.kind==XL6_TK_RPAREN || token.kind==XL6_TK_RBRACKET || token.kind==XL6_TK_RBRACE){if(depth)--depth;}
        if(!xlsSignatureToken(stream,token,query))return 0;
        if(token.kind==XL6_TK_EOF)break;
    }
    if(!projected){query->error="signature call was not projected";return 0;}
    return xlsParserFinishTokens(stream,diags);
}
static int xlsSignatureCompletionToken(XL6_ParseTokenStream* stream,const char* source,XL6_CompletionQuery* query)
{
    xls_signature_scope* scope=xlsSignatureScopeCurrent(query);
    if(!scope)return xl6ParserCompletionToken(stream,source,query);
    for(size_t i=0;i<stream->count;++i){const XL6_Token* token=&stream->tokens[i];
        if(token->kind==XL6_TK_LPAREN && token->startOffset==scope->data->site.argumentsStart-1){query->replacement=xl6AstRangeFromToken(token);query->prefix=xrtStrDup("");return query->replacement.known && query->prefix;}
    }
    query->error="signature call has no original source token";return 0;
}
