#ifndef XIDE_SEMANTIC_COMPLETION_H
#define XIDE_SEMANTIC_COMPLETION_H
#include "services/semantic_definition.h"
/* Caller owns a standard CompletionList. Input sources are borrowed while
   validating all byte/scalar ranges in one sorted pass per source. */
xvalue* xideSemanticCompletion(const xvalue* query,const xide_semantic_source* sources,size_t count,const char* path,int offset,int version,int available,xide_model_error* error);
#endif