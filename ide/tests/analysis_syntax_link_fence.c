/* Isolated syntax qualification has no semantic raw-body consumer installed.
 * PE refptr sections retain its optional callbacks even after --gc-sections.
 * Abort if a fixture enters that unqualified path; never emulate its behavior.
 * Full language-service builds link the real frontend_raw_body.c instead. */
#include "analysis/frontend_raw_body.h"
#include <stdio.h>
#include <stdlib.h>
int xlsRawBodyAdmit(xls_raw_unit* unit,const char* path,const XL6_AstRange* range,size_t* bytes,void* user)
{
    (void)unit;(void)path;(void)range;(void)bytes;(void)user;
    fputs("isolated syntax fixture entered semantic raw-body admission\n",stderr);abort();
}
void xlsRawBodyRecord(xls_raw_unit* unit,const char* path,XL6_AstNode* node,int deferred,void* user)
{
    (void)unit;(void)path;(void)node;(void)deferred;(void)user;
    fputs("isolated syntax fixture entered semantic raw-body publication\n",stderr);abort();
}
