/* Real ELF objects and archives; public API return values must report errors. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"
static const char *library_root;
static int checks, failures, before_control;
static char diagnostic[2048];
static void error_sink(void *unused,const char *text)
{
    (void)unused;snprintf(diagnostic,sizeof diagnostic,"%s",text);
}
static void check(int ok,const char *name)
{
    ++checks;if(!ok){++failures;fprintf(stderr,"FAIL %s: %s\n",name,diagnostic);}
}
static TCCState *state(int mode)
{
    TCCState *s=tcc_new();diagnostic[0]=0;tcc_set_error_func(s,NULL,error_sink);
    tcc_set_lib_path(s,library_root);
    check(!tcc_set_options(s,"-nostdlib -ffunction-sections -fdata-sections"),"ordinary options selected");
    check(!tcc_set_output_type(s,mode),"requested output selected");return s;
}
static void conflict(TCCState *s,int rc,int previous,const char *name)
{
    check(s->nb_errors>previous && strstr(diagnostic,"'binary_value' defined twice"),"genuine duplicate definition diagnosed without abort");
    check(before_control?rc==0:rc<0,name);
}
int main(int argc,char **argv)
{
    char first[2048],second[2048],archive[2048];int i,rc,previous;TCCState*s;
    if(argc<3||argc>4)return 2;library_root=argv[1];before_control=argc==4;
    snprintf(first,sizeof first,"%s/first.o",argv[2]);snprintf(second,sizeof second,"%s/second.o",argv[2]);
    snprintf(archive,sizeof archive,"%s/libduplicate.a",argv[2]);
    for(i=0;i<2;++i){
        s=state(TCC_OUTPUT_OBJ);
        check(!tcc_compile_string(s,i?"int binary_value(void){return 42;}":"int binary_value(void){return 41;}"),"conflicting object source compiled");
        check(!tcc_output_file(s,i?second:first),"ordinary object serialized");tcc_delete(s);
    }
    if(argc==3 || strcmp(argv[3],"before")==0){
        /* Python constructs the archive between the emit and verify runs. */
        printf("BINARY_OBJECTS_EMITTED\n");return failures?1:0;
    }
    before_control=!strcmp(argv[3],"verify-before");
    s=state(TCC_OUTPUT_MEMORY);check(!tcc_add_file(s,first),"first object admitted");
    previous=s->nb_errors;rc=tcc_add_file(s,second);conflict(s,rc,previous,"public object load reports its new linker errors");tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY);check(!tcc_set_options(s,"-Wl,--whole-archive"),"whole archive selected");
    previous=s->nb_errors;rc=tcc_add_file(s,archive);conflict(s,rc,previous,"public archive load reports nested member errors");tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY);check(!tcc_set_options(s,"-Wl,--whole-archive"),"library whole archive selected");
    check(!tcc_add_library_path(s,argv[2]),"actual library directory selected");
    previous=s->nb_errors;rc=tcc_add_library(s,"duplicate");conflict(s,rc,previous,"library search reports nested object errors");tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY);check(!tcc_add_file(s,first),"fresh state admits valid input after failed state teardown");
    rc=tcc_relocate(s);check(!rc,"valid object relocates without hidden errors");
    if(!rc){int(*value)(void)=tcc_get_symbol(s,"binary_value");check(value&&value()==41,"independent valid object executes");}
    check(!s->nb_errors,"successful input has no hidden errors");tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY);
    {
        char missing[2048];snprintf(missing,sizeof missing,"%s/not-present.o",argv[2]);
        check(tcc_add_file(s,missing)<0 && s->nb_errors>0,"ordinary missing-file failure is delivered");
        previous=s->nb_errors;
        check(!tcc_add_file(s,first) && s->nb_errors==previous,"an earlier diagnosed error does not poison a later valid binary load");
    }
    tcc_delete(s);
    printf("BINARY_ERROR_CHECKS=%d FAILURES=%d BEFORE=%d\n",checks,failures,before_control);return failures?1:0;
}
