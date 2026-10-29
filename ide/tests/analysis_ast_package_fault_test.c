#define main packageOrdinaryMain
#include "analysis_ast_package_test.c"
#undef main
static const char* target;
static unsigned refusals;
static char* gatedDup(const char* text)
{if(text==target){++refusals;return NULL;}return xrtStrDup(text);}
#define xrtStrDup gatedDup
#include "../src/analysis/ast_clone.c"
#undef xrtStrDup
static int faults(void)
{
    XL6_AstNode* input=make();CHECK(input);uint64 baseline=live();size_t references=input->source->references;
    const XL6_AstNode* nodes[]={input->children[0],input->children[1],input->children[0]->inferredType};
    for(size_t i=0;i<3;++i)for(unsigned pool=0;pool<2;++pool){
        target=nodes[i]->compiledPackageLink;unsigned before=refusals;
        XL6_AstNode* out=xlsAstClonePooled(input,NULL,NULL,NULL,pool ? 1024u*1024u : 0,NULL,NULL);
        CHECK(!out && refusals==before+1 && live()==baseline && input->source->references==references);
        target=NULL;out=xlsAstClonePooled(input,NULL,NULL,NULL,pool ? 1024u*1024u : 0,NULL,NULL);
        CHECK(out && equal(input,out));xl6AstFree(out);CHECK(live()==baseline);
    }
    xl6AstFree(input);CHECK(refusals==6);return 1;
}
int main(void)
{xrtMemStatsEnable(true);if(!faults())return 1;printf("analysis_ast_package_fault_test passed: %u checks;6targetedlinkduplicatefailures,balancedpartial/source/pool cleanupandretry;notallocator-wideOOM.\n",checks);return 0;}
