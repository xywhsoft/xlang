#include "src/stdlib/xl6_stdlib_data.h"
#include "src/parser/xl6_parser_api.h"
#include "xl.h"
#include <stdio.h>
#include <stdlib.h>
/* Compile the unchanged expander with a parse observer. The observer captures
 * exactly the declarations selected by its real dependency/feature logic;
 * it never substitutes parsing, ownership, or declaration generation. */
static FILE* declaration_output;
static unsigned declaration_captures;
static int capture_parse(const char* text,const char* name,XL6_ParseResult* result)
{
    if(declaration_output && !strcmp(name,"<stdlib>")) {
        size_t length=strlen(text);
        if(fwrite(text,1,length,declaration_output)!=length) return 0;
        ++declaration_captures;
    }
    return xl6ParseSource(text,name,result);
}
#define xl6ParseSource capture_parse
#define xl6StdlibExpandModule catalog_expand
#define xl6StdlibImplicitMethodInternalName catalog_implicit_method
#include "src/stdlib/xl6_stdlib.c"
#undef xl6ParseSource
#undef xl6StdlibExpandModule
#undef xl6StdlibImplicitMethodInternalName

static int expand(const char* input,const char* output)
{
    FILE* in=fopen(input,"rb"); if(!in) return 1;
    if(fseek(in,0,SEEK_END)) {fclose(in);return 1;}
    long length=ftell(in);
    if(length<0 || length>16*1024*1024 || fseek(in,0,SEEK_SET)) {fclose(in);return 1;}
    char* text=malloc((size_t)length+1);
    if(!text) {fclose(in);return 1;}
    int okay=fread(text,1,(size_t)length,in)==(size_t)length;
    if(fclose(in)) okay=0;
    text[length]=0;
    if(!okay || !xlRuntimeInit()) {free(text);return 1;}
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
    okay=xl6ParseSource(text,input,&parsed);free(text);
    declaration_output=okay?fopen(output,"wb"):NULL;
    if(okay && !declaration_output) okay=0;
    if(okay) okay=catalog_expand(parsed.root,&parsed.diags);
    if(declaration_output && fclose(declaration_output)) okay=0;
    declaration_output=NULL;
    if(!okay) xl6DiagPrintText(&parsed.diags,input);
    xl6ParseResultFree(&parsed);xlRuntimeUnit();
    return okay && declaration_captures==1 ? 0 : 1;
}
static void json(FILE* out,const char* text)
{
    fputc('"',out); if(text) for(const unsigned char* p=(const unsigned char*)text;*p;++p) {
        if(*p=='"' || *p=='\\') { fputc('\\',out); fputc(*p,out); }
        else if(*p<32) fprintf(out,"\\u%04x",*p); else fputc(*p,out);
    } fputc('"',out);
}
int main(int argc,char** argv)
{
    if(argc==4 && !strcmp(argv[1],"--expand")) return expand(argv[2],argv[3]);
    if(argc!=2) return 2; FILE* out=fopen(argv[1],"wb"); if(!out) return 1;
    fputs("{\"functions\":[",out);
    for(size_t i=0;i<xl6StdlibFuncCount;++i) {
        const XL6_StdlibFunc* item=&xl6StdlibFuncs[i]; if(i) fputc(',',out);
        fputs("{\"namespace\":",out); json(out,item->ns); fputs(",\"name\":",out); json(out,item->name);
        fputs(",\"internal\":",out); json(out,item->internalName); fputs(",\"return\":",out); json(out,item->returnType);
        fputs(",\"body\":",out); json(out,item->sourceBody); fputs(",\"parameters\":[",out);
        for(size_t j=0;j<item->paramCount;++j) { if(j) fputc(',',out); fputs("{\"type\":",out); json(out,item->params[j].type); fputs(",\"name\":",out); json(out,item->params[j].name); fputc('}',out); }
    fputs("]}",out);
    }
    fputs("],\"constants\":[",out);
    for(size_t i=0;i<xl6StdlibConstCount;++i) { if(i) fputc(',',out); fputs("{\"namespace\":",out); json(out,xl6StdlibConsts[i].ns); fputs(",\"name\":",out); json(out,xl6StdlibConsts[i].name); fputs(",\"value\":",out); json(out,xl6StdlibConsts[i].value); fputc('}',out); }
    fputs("],\"sourceChunks\":[",out); int first=1;
#define CHUNK(value) do { if(!first) fputc(',',out); first=0; fputs("{\"symbol\":\"" #value "\",\"source\":",out); json(out,value); fputc('}',out); } while(0)
#include "stdlib_catalog_chunks.inc"
    fputs("]}",out); int okay=!ferror(out); if(fclose(out)) okay=0; return okay ? 0 : 1;
}
