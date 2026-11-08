/* Lexical highlighting, not a schema validator. XLON extends JSON with the
 * XRT tag grammar; line formats reset unfinished input at physical newlines. */
enum { DATA_XLON=1,DATA_LINES=2 };
static int dataDigit(unsigned char c){return c>='0' && c<='9';}
static int dataNameStart(unsigned char c){return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_';}
static int dataName(unsigned char c){return dataNameStart(c) || dataDigit(c) || c=='.' || c=='-';}
static int dataSpace(unsigned char c){return c==' ' || c=='\t' || c=='\r' || c=='\n';}
static int dataBoundary(unsigned char c){return dataSpace(c) || strchr("{}[](),:\"/",c)!=NULL;}
static int dataNumberValid(const char* s,int start,int end)
{
    int i=start;if(i<end && s[i]=='-')++i;
    if(i==end)return 0;
    if(s[i]=='0')++i;
    else {if(s[i]<'1' || s[i]>'9')return 0;while(i<end && dataDigit((unsigned char)s[i]))++i;}
    if(i<end && s[i]=='.'){int first=++i;while(i<end && dataDigit((unsigned char)s[i]))++i;if(i==first)return 0;}
    if(i<end && (s[i]=='e' || s[i]=='E')){++i;if(i<end && (s[i]=='+' || s[i]=='-'))++i;int first=i;while(i<end && dataDigit((unsigned char)s[i]))++i;if(i==first)return 0;}
    return i==end;
}
static int dataString(const char* text,int length,int* offset)
{
    int i=*offset+1,valid=1,closed=0;
    while(i<length){
        unsigned char c=(unsigned char)text[i];
        if(c=='\r' || c=='\n')break;
        ++i;
        if(c=='"'){closed=1;break;}
        if(c<0x20){valid=0;continue;}
        if(c!='\\')continue;
        if(i>=length || text[i]=='\r' || text[i]=='\n'){valid=0;break;}
        c=(unsigned char)text[i++];
        if(c=='u'){
            for(int k=0;k<4;++k){
                if(i>=length || !isxdigit((unsigned char)text[i])){valid=0;break;}
                ++i;
            }
        }else if(!strchr("\"\\/bfnrt",c))valid=0;
    }
    *offset=i;return valid && closed;
}
static int dataLexInto(xui_code_document document,int startLine,int endLine,xui_code_token_t* tokens,int capacity,int* count,void* user,xarray* output)
{
    if(!document || !count || capacity<0 || !user)return XUI_ERROR_INVALID_ARGUMENT;
    unsigned flags=*(const unsigned*)user;*count=0;
    int lines=xuiCodeDocumentGetLineCount(document),begin,end;
    if(startLine<0)startLine=0;
    if(endLine<0 || endLine>=lines)endLine=lines-1;
    if(startLine>endLine || startLine>=lines)return XUI_OK;
    int result=xuiCodeDocumentGetLineRange(document,startLine,&begin,NULL);
    if(result==XUI_OK)result=xuiCodeDocumentGetLineRange(document,endLine,NULL,&end);
    if(result!=XUI_OK)return result;
    const char* text=xuiCodeDocumentGetText(document);int length=xuiCodeDocumentGetLength(document);
    for(int i=(flags&DATA_LINES)?begin:0;i<length && i<end;){
        unsigned char c=(unsigned char)text[i];int start=i,kind=XUI_CODE_TOKEN_ERROR;
        if(dataSpace(c)){++i;continue;}
        if(c=='"')kind=dataString(text,length,&i)?XUI_CODE_TOKEN_STRING:XUI_CODE_TOKEN_ERROR;
        else if(c=='/' && i+1<length && text[i+1]=='/'){
            i+=2;while(i<length && text[i]!='\r' && text[i]!='\n')++i;
            if(flags&DATA_XLON)kind=XUI_CODE_TOKEN_COMMENT;
        }else if(c=='/' && i+1<length && text[i+1]=='*'){
            int closed=0;i+=2;
            while(i<length){
                if(i+1<length && text[i]=='*' && text[i+1]=='/'){i+=2;closed=1;break;}
                if((flags&DATA_LINES) && (text[i]=='\r' || text[i]=='\n'))break;
                ++i;
            }
            if(closed && (flags&DATA_XLON))kind=XUI_CODE_TOKEN_COMMENT;
        }else if(dataDigit(c) || c=='-' || c=='+'){
            ++i;while(i<length && !dataBoundary((unsigned char)text[i]))++i;
            if(dataNumberValid(text,start,i))kind=XUI_CODE_TOKEN_NUMBER;
        }else if(dataNameStart(c)){
            ++i;while(i<length && dataName((unsigned char)text[i]))++i;
            if(word(text,start,i,"true") || word(text,start,i,"false") || word(text,start,i,"null"))kind=XUI_CODE_TOKEN_KEYWORD;
            else if((flags&DATA_XLON) && i-start<=128 && i<length &&
                (text[i]=='(' || (text[i]=='[' && word(text,start,i,"set")) || (text[i]=='{' && word(text,start,i,"intmap"))))kind=XUI_CODE_TOKEN_TYPE;
        }else{
            ++i;
            if(strchr("{}[]",c) || ((flags&DATA_XLON) && (c=='(' || c==')')))kind=XUI_CODE_TOKEN_BRACE;
            else if(c==',' || c==':')kind=XUI_CODE_TOKEN_OPERATOR;
        }
        if(output && i>begin && start<end){xui_code_token_t t={start,i,kind,kind,0};if(*count==INT_MAX || !xrtArrayAppend(output,&t,1))return XUI_ERROR_OUT_OF_MEMORY;++*count;}
        else emit(tokens,capacity,count,start,i,kind,begin,end);
    }
    return XUI_OK;
}
static int dataLex(xui_code_document document,int start,int end,xui_code_token_t* tokens,int capacity,int* count,void* user)
{return dataLexInto(document,start,end,tokens,capacity,count,user,NULL);}
static int dataFoldTokens(xui_code_document document,const xui_code_token_t* tokens,int tokenCount,xui_code_fold_range_t* ranges,int capacity,int* count,void* user,xarray* output)
{
    *count=0;if(*(const unsigned*)user&DATA_LINES)return XUI_OK;
    typedef struct data_open {int line;char bracket;} data_open;
    xarray stack;if(!xrtArrayInit(&stack,sizeof(data_open)))return XUI_ERROR_OUT_OF_MEMORY;
    if(!xrtArrayReserve(&stack,(size_t)tokenCount)){xrtArrayUnit(&stack);return XUI_ERROR_OUT_OF_MEMORY;}
    const char* text=xuiCodeDocumentGetText(document);int offset=0,line=0,depth=0,result=XUI_OK;
    for(int i=0;i<tokenCount && result==XUI_OK;++i){
        const xui_code_token_t* t=tokens+i;int first=lineAt(text,&offset,&line,t->iStartOffset),last=lineAt(text,&offset,&line,t->iEndOffset),foldStart=-1,level=depth;uint32_t flags=0;
        if(t->iKind==XUI_CODE_TOKEN_COMMENT && last>first){foldStart=first;flags=XUI_CODE_FOLD_COMMENT;}
        else if(t->iKind==XUI_CODE_TOKEN_BRACE){
            char c=text[t->iStartOffset];
            if(c=='{' || c=='[' || c=='(')((data_open*)stack.Data)[depth++]=(data_open){first,c};
            else if(depth){data_open open=((data_open*)stack.Data)[depth-1];
                if((open.bracket=='{' && c=='}') || (open.bracket=='[' && c==']') || (open.bracket=='(' && c==')')){--depth;foldStart=open.line;level=depth;}
            }
        }
        if(foldStart>=0 && last>foldStart){
            if(output){xui_code_fold_range_t f={sizeof(f),foldStart,last,level,flags|XUI_CODE_FOLD_HEADER};if(*count==INT_MAX || !xrtArrayAppend(output,&f,1))result=XUI_ERROR_OUT_OF_MEMORY;else ++*count;}
            else foldEmit(ranges,capacity,count,foldStart,last,level,flags);
        }
    }
    xrtArrayUnit(&stack);return result;
}
static int dataFold(xui_code_document document,xui_code_fold_range_t* ranges,int capacity,int* count,void* user)
{
    if(!document || !count || capacity<0 || !user)return XUI_ERROR_INVALID_ARGUMENT;
    *count=0;if(*(const unsigned*)user&DATA_LINES)return XUI_OK;
    xarray tokens;if(!xrtArrayInit(&tokens,sizeof(xui_code_token_t)))return XUI_ERROR_OUT_OF_MEMORY;
    int n=0,result=dataLexInto(document,0,-1,NULL,0,&n,user,&tokens);
    if(result==XUI_OK)result=dataFoldTokens(document,(const xui_code_token_t*)tokens.Data,n,ranges,capacity,count,user,NULL);
    xrtArrayUnit(&tokens);return result;
}
