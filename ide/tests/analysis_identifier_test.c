#include "analysis/identifier.h"
#include "src/lexer/xl6_lexer_api.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_identifier_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int original(const char* text,size_t length)
{
 char buffer[256];if(length+2>sizeof(buffer))return -1;
 buffer[0]=' ';memcpy(buffer+1,text,length);buffer[length+1]=0;
 XL6_Lexer lexer;XL6_Token first,last;
 if(!xl6LexerOpen(&lexer,buffer))return -1;
 first=xl6LexerNext(&lexer);last=xl6LexerNext(&lexer);
 int valid=first.kind==XL6_TK_IDENT && first.startOffset==1 && (size_t)first.endOffset==length+1 && last.kind==XL6_TK_EOF;
 xl6LexerClose(&lexer);return valid;
}
static int compare(const char* text,size_t n)
{int expected=original(text,n),actual=xlsIdentifierValid(text,n);if(expected!=actual)fprintf(stderr,"oracle mismatch length=%zu first=%02x expected=%d actual=%d\n",n,n?(unsigned char)text[0]:0,expected,actual);CHECK(expected>=0 && actual==expected);return 1;}
static int32 reader(void* user)
{int* okay=user;*okay=1;for(unsigned i=0;i<10000;++i)if(!xlsIdentifierValid("变量😀_2",strlen("变量😀_2")) || xlsIdentifierValid("class",5) || xlsIdentifierValid("a.b",3))*okay=0;return 0;}
static int run(void)
{
 CHECK(!xlsIdentifierValid(NULL,0));CHECK(!xlsIdentifierValid(NULL,1));CHECK(!xlsIdentifierValid("",0));
 char ascii[2];for(int a=1;a<128;++a){ascii[0]=(char)a;CHECK(compare(ascii,1));for(int b=1;b<128;++b){ascii[1]=(char)b;CHECK(compare(ascii,2));}}
 const char* cases[]={"_","__","int","string","Int","func","Func","class","CLASS","end","return","import","export","as","and","or","not","true","false","null","none","self","this","typedef","typeof","const","var","auto","async","await","yield","if","then","while","for","in","assert","break","continue","try","catch","throw","finally","c_callback","c_vararg","保留字","éclair","😀","😀a","a😀2","é","\xef\xbb\xbf","a\xef\xbb\xbf","a b","a\nb","a\r\nb","a.b","a::b","1e5","0xFF","'a'","\"a\"","r\"a\"","f\"a\"","a#b","a//b","a/*b*/","a\t","a+","a-"," 变量","\xe2\x80\x8b"};
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i)CHECK(compare(cases[i],strlen(cases[i])));
 const char* bad[]={"a\xc0\xaf","\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82","a\xff"};for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i)CHECK(!xlsIdentifierValid(bad[i],strlen(bad[i])));
 char zero[]={'a',0,'b'};CHECK(!xlsIdentifierValid(zero,sizeof(zero)));
 char* maximum=xrtMalloc(XLS_IDENTIFIER_BYTES+1);CHECK(maximum);memset(maximum,'a',XLS_IDENTIFIER_BYTES+1);CHECK(xlsIdentifierValid(maximum,XLS_IDENTIFIER_BYTES));CHECK(!xlsIdentifierValid(maximum,XLS_IDENTIFIER_BYTES+1));maximum[XLS_IDENTIFIER_BYTES-1]='.';CHECK(!xlsIdentifierValid(maximum,XLS_IDENTIFIER_BYTES));xrtFree(maximum);
 int okay[4]={0};xthread* threads[4];for(int i=0;i<4;++i){threads[i]=xrtThreadCreate(reader,okay+i,0);CHECK(threads[i]);}for(int i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && okay[i]);xrtThreadDestroy(threads[i]);}
 return 1;
}
int main(void){if(!run())return 1;printf("analysis_identifier_test passed: %u checks; actual native scanner oracle, ASCII pairs, keywords, UTF8, quota and concurrent immutable DFA\n",checks);return 0;}
