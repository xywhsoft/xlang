/* Test-only counted producer; no production forwarding implementation. */
#include "foreign_product_nullable_lift.h"
#include "foreign_output_snapshots.c"
static int nullableReads;
int nullableProductRead(OutputValueRoot* root,int mode,OutputValueRecord* out)
{++nullableReads;return outputValueRead(root,mode,out);}
int nullableProductReadCalls(void){return nullableReads;}
