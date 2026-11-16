#define main priorPersistentMain
#include "analysis_ir_persistent_test.c"
#undef main

typedef struct node_view_reader {xls_ir_result* view;const char* expected;int okay;} node_view_reader;
static int32 retireView(void* opaque)
{
 node_view_reader* reader=opaque;char* actual=dump(xlsFrontendIrResultModule(reader->view));
 reader->okay=actual && !strcmp(actual,reader->expected);
 xrtFree(actual);xlsFrontendIrResultDestroy(reader->view);return reader->okay ? 0 : 1;
}
static int sameView(void)
{
 char* path=xrtPathAbs("node-view-中文.xl");char* text=body(0);CHECK(path && text);
 xls_semantic_check* source=baseChecked(path,text);CHECK(source && xlsSemanticCheckInfo(source).accepted);
 const XL6_AstNode* root=xlsSemanticCheckAst(source);
 xls_ast_clone_options clone={.bytes=64u*1024u*1024u};XL6_AstNode* independent=xlsAstClone(root,&clone,NULL);CHECK(independent);
 xls_ir_result* first=persistentBuild(root,NULL,32u*1024u*1024u);CHECK(first && xlsFrontendIrResultPersistentWork(first).captured);
 const XL6_IrNode* arena=xlsFrontendIrResultModule(first)->nodes;
 xls_ir_result* second=persistentBuild(independent,first,32u*1024u*1024u);CHECK(second);
 CHECK(xlsFrontendIrResultPersistentWork(second).reused && !xlsFrontendIrResultPersistentWork(second).nodesCopied);
 CHECK(xlsFrontendIrResultPersistentWork(second).nodesShared==xlsFrontendIrResultModule(first)->nodeCount &&
       xlsFrontendIrResultPersistentWork(second).certificatesReused==1 && !xlsFrontendIrResultPersistentWork(second).verified);
 CHECK(xlsFrontendIrResultModule(second)->nodes==arena);CHECK(persistentEqual(second,independent));
 xlsFrontendIrResultDestroy(first);xlsSemanticCheckDestroy(source);
 CHECK(persistentEqual(second,independent));
 for(unsigned i=0;i<16;++i){
  xls_ir_result* next=persistentBuild(independent,second,32u*1024u*1024u);CHECK(next);
  CHECK(xlsFrontendIrResultModule(next)->nodes==arena && !xlsFrontendIrResultPersistentWork(next).nodesCopied);
  xlsFrontendIrResultDestroy(second);second=next;CHECK(persistentEqual(second,independent));
 }
 int delta=11;clone.transform=shiftedNode;clone.user=&delta;
 XL6_AstNode* moved=xlsAstClone(independent,&clone,NULL);CHECK(moved);
 xls_ir_result* relocated=persistentBuild(moved,second,32u*1024u*1024u);CHECK(relocated);
 CHECK(xlsFrontendIrResultModule(relocated)->nodes!=arena && xlsFrontendIrResultPersistentWork(relocated).nodesCopied>0);
 CHECK(persistentEqual(relocated,moved));CHECK(persistentEqual(second,independent));
 const XL6_IrNode* relocatedArena=xlsFrontendIrResultModule(relocated)->nodes;
 xls_ir_result* held=persistentBuild(moved,relocated,32u*1024u*1024u);CHECK(held);
 CHECK(xlsFrontendIrResultModule(held)->nodes==relocatedArena && !xlsFrontendIrResultPersistentWork(held).nodesCopied);
 xlsFrontendIrResultDestroy(second);xlsFrontendIrResultDestroy(relocated);xl6AstFree(independent);
 CHECK(persistentEqual(held,moved));
 xls_ir_result* zero=persistentBuild(moved,held,0);CHECK(zero);
 CHECK(!xlsFrontendIrResultPersistentWork(zero).reused && xlsFrontendIrResultModule(zero)->nodes!=relocatedArena);
 CHECK(persistentEqual(zero,moved));xlsFrontendIrResultDestroy(zero);
 node_view_reader readers[4];xthread* threads[4];char* expected=dump(xlsFrontendIrResultModule(held));CHECK(expected);
 for(unsigned i=0;i<4;++i){readers[i]=(node_view_reader){persistentBuild(moved,held,32u*1024u*1024u),expected,0};CHECK(readers[i].view && xlsFrontendIrResultModule(readers[i].view)->nodes==relocatedArena);}
 xlsFrontendIrResultDestroy(held);xl6AstFree(moved);
 for(unsigned i=0;i<4;++i){threads[i]=xrtThreadCreate(retireView,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xrtFree(expected);xrtFree(path);xrtFree(text);return 1;
}
int main(void)
{xrtAtomic32Init(&checks,0);if(!sameView())return 1;printf("node-view passed: %u complete IR/ownership checks.\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;}
