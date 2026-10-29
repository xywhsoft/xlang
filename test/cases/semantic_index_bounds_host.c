/* Real formatter/index code linked to one DLL's XRT; malformed ASTs test limits only. */
#include "src/sem/xl6_sem_index.h"
#include "src/type/xl6_type_model.h"
#include <stdio.h>
#include <stdlib.h>
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "semantic_index_bounds_host:%d: %s\n", __LINE__, #c); exit(1); } } while (0)
int main(void)
{
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, "int64", 5, NULL);
    XL6_SourceReport report;
    char error[192]; char* json; char* canonical;
    memset(&report, 0, sizeof(report)); report.complete = 1;
    REQUIRE(root && type);
    canonical = xl6TypeToCanonicalStringLimited(root, type, 3, 1, 1);
    REQUIRE(canonical && strcmp(canonical, "int") == 0); xrtFree(canonical);
    REQUIRE(!xl6TypeToCanonicalStringLimited(root, type, 2, 1, 1));
    REQUIRE(!xl6TypeToCanonicalStringLimited(root, type, 0, 1, 1));
    xl6AstAddChild(root, type);
    json = xl6SemIndexJson(root, &report, error, sizeof(error));
    REQUIRE(json && strstr(json, "\"canonical\":\"int\""));
    REQUIRE(strstr(json, "\"sourceId\":0,\"range\":null")); xrtFree(json);
    report.complete = 0;
    REQUIRE(!xl6SemIndexJson(root, &report, error, sizeof(error)) && *error);
    report.complete = 1;
    { XL6_AstNode* cursor = root;
      for (size_t i=0; i<258; ++i) { XL6_AstNode* node = xl6AstNew(XL6_AST_BLOCK, NULL); REQUIRE(node); xl6AstAddChild(cursor, node); cursor=node; }
      REQUIRE(!xl6SemIndexJson(root, &report, error, sizeof(error)) && strstr(error,"tree limit")); }
    xl6AstFree(root);
    root = xl6AstNew(XL6_AST_MODULE, NULL); REQUIRE(root);
    for (size_t i=0; i<6000; ++i) { XL6_AstNode* node=xl6AstNew(XL6_AST_NAME,NULL); REQUIRE(node); xl6AstAddChild(root,node); }
    REQUIRE(!xl6SemIndexJson(root, &report, error, sizeof(error)) && strstr(error,"byte limit"));
    xl6AstFree(root);
    root = xl6AstNew(XL6_AST_MODULE, NULL); REQUIRE(root);
    { XL6_AstNode* array=xl6AstNewText(XL6_AST_TYPE,"array",5,NULL);
      XL6_AstNode* args=xl6AstNew(XL6_AST_LIST,NULL);
      XL6_AstNode* leaf=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);
      REQUIRE(array&&args&&leaf); xl6AstAddChild(array,args); xl6AstAddChild(args,leaf);
      REQUIRE(!xl6TypeToCanonicalStringLimited(root,array,32,1,10));
      REQUIRE(!xl6TypeToCanonicalStringLimited(root,array,32,10,1));
      canonical=xl6TypeToCanonicalStringLimited(root,array,32,10,10);
      REQUIRE(canonical&&strcmp(canonical,"array<int>")==0); xrtFree(canonical); xl6AstFree(array); }
    json=xl6SemIndexJson(root,&report,error,sizeof(error)); REQUIRE(json); xrtFree(json); xl6AstFree(root);
    puts("semantic_index_bounds_host passed"); return 0;
}
