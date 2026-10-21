#include "src/stdlib/xl6_stdlib_data.h"
#include <stdio.h>
#include <stdlib.h>
static void json(FILE* out,const char* text)
{
    fputc('"',out); if(text) for(const unsigned char* p=(const unsigned char*)text;*p;++p) {
        if(*p=='"' || *p=='\\') { fputc('\\',out); fputc(*p,out); }
        else if(*p<32) fprintf(out,"\\u%04x",*p); else fputc(*p,out);
    } fputc('"',out);
}
int main(int argc,char** argv)
{
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