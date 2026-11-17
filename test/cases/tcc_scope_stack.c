/* Actual provider CPU gate. Baseline large frames are inspected, never run. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"
#include <assert.h>

static const char *base;
static int errors;
static void diagnostic(void *user,const char *message)
{(void)user;fprintf(stderr,"%s\n",message);++errors;}
static TCCState *state(void)
{
 TCCState *s=tcc_new();assert(s);tcc_set_error_func(s,NULL,diagnostic);
 tcc_set_lib_path(s,base);assert(!tcc_set_options(s,"-nostdlib"));
 assert(!tcc_set_output_type(s,TCC_OUTPUT_MEMORY));
 tcc_add_symbol(s,"memset",memset);char path[1024];snprintf(path,sizeof path,"%s/lib/alloca.S",base);assert(!tcc_add_file(s,path));return s;
}
static const char functional[]=
 "void *alloca(__SIZE_TYPE__);struct Pair {long long a,b;};\n"
 "static struct Pair pair(long long a,long long b){struct Pair p={a,b};return p;}\n"
 "static long long sum(struct Pair a,struct Pair b){return a.a+a.b+b.a+b.b;}\n"
 "static int drops;static void drop(int *p){drops+=*p;}\n"
 "int entry(void){int outside=37, total=0;int *saved=&outside;\n"
 "{volatile char first[257];first[0]=3;first[256]=4;total+=first[0]+first[256];}\n"
 "{volatile long long second[97];second[0]=5;second[96]=6;total+=(int)(second[0]+second[96]);}\n"
 "if(*saved!=37 || total!=18)return 1;\n"
 "for(int i=0;i<9;i++){int n=i+3;volatile int v[n];v[0]=i;v[n-1]=n;total+=v[0]+v[n-1];}\n"
 "if(total!=117 || *saved!=37)return 2;\n"
 "if(sum(pair(1,2),({int x=9;pair(x,4);}))!=16)return 3;\n"
 "{int x __attribute__((cleanup(drop)))=7;goto after;}after:;\n"
 "if(drops!=7)return 4;\n"
 "{int *p=alloca(23*sizeof(int));p[0]=19;p[22]=23;saved=p;}\n"
 "{volatile int ordinary[31];ordinary[0]=11;ordinary[30]=17;total+=ordinary[0]+ordinary[30];}\n"
 "if(saved[0]!=19 || saved[22]!=23 || total!=145)return 5;\n"
 "{int a=17;total+=({struct Pair p=pair(a,13);p.a+p.b;})+({int b=9;b+2;});}\n"
 "return total==186?0:6;}\n";
int main(int argc,char **argv)
{
 assert(argc==3);base=argv[1];int fixed=atoi(argv[2]);
 TCCState *s=state();assert(!tcc_compile_string(s,functional));
 assert(!tcc_relocate(s));int (*entry)(void)=tcc_get_symbol(s,"entry");assert(entry);
 int status=entry();printf("FUNCTIONAL_STATUS=%d\n",status);assert(status==0);tcc_delete(s);
 s=state();size_t capacity=400000,used=0;static char program[400000];
 used+=snprintf(program+used,capacity-used,"int entry(void){int total=0;\n");
 for(int i=0;i<2048;i++)used+=snprintf(program+used,capacity-used,
  "{volatile unsigned char bytes[1024];bytes[0]=%d;bytes[1023]=%d;total+=bytes[0]+bytes[1023];}\n",i%127,(i+1)%127);
 used+=snprintf(program+used,capacity-used,"return total;}\n");assert(used<capacity);
 assert(!tcc_compile_string(s,program));
 long frame=-(long)loc;printf("STACK_FRAME_BYTES=%ld\n",frame);fflush(stdout);
 if(fixed){assert(frame<8192);assert(!tcc_relocate(s));entry=tcc_get_symbol(s,"entry");assert(entry);int expected=0;for(int i=0;i<2048;i++)expected+=i%127+(i+1)%127;assert(entry()==expected);}
 else assert(frame>2*1024*1024);
 tcc_delete(s);assert(!errors);puts("SCOPE_STACK_PROBE_OK");return 0;
}
