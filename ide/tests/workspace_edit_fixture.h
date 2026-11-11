#ifndef WORKSPACE_EDIT_FIXTURE_H
#define WORKSPACE_EDIT_FIXTURE_H
#include "protocol/workspace_edit.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace_edit_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static inline xvalue* parseEdit(const char* s){return xrtJsonParse(xrtStrView(s));}
static inline int putEdit(xvalue* o,const char* k,xvalue* v){return v && xrtValueObjectSetNew(o,xrtStrView(k),v);}
static inline xvalue* editPosition(int line,int character){xvalue* p=xrtValueObject();if(p && putEdit(p,"line",xrtValueInt(line)) && putEdit(p,"character",xrtValueInt(character)))return p;xrtValueRelease(p);return NULL;}
static inline xvalue* textEdit(int line,int start,int end,const char* text)
{xvalue *o=xrtValueObject(),*range=xrtValueObject();int ok=o && range && putEdit(range,"start",editPosition(line,start)) && putEdit(range,"end",editPosition(line,end));if(ok){ok=putEdit(o,"range",range);range=NULL;}if(ok)ok=putEdit(o,"newText",xrtValueString(xrtStrView(text)));xrtValueRelease(range);if(!ok){xrtValueRelease(o);o=NULL;}return o;}
static inline xvalue* documentEdit(const char* uri,int hasVersion,int version,xvalue* edits)
{xvalue *o=xrtValueObject(),*doc=xrtValueObject();int ok=o && doc && putEdit(doc,"uri",xrtValueString(xrtStrView(uri))) && putEdit(doc,"version",hasVersion?xrtValueInt(version):xrtValueNull());if(ok){ok=putEdit(o,"textDocument",doc);doc=NULL;}if(ok){ok=putEdit(o,"edits",edits);edits=NULL;}xrtValueRelease(doc);xrtValueRelease(edits);if(!ok){xrtValueRelease(o);o=NULL;}return o;}
static inline xvalue* oneEdit(const char* uri,int hasVersion,int version,const char* replacement)
{xvalue *root=xrtValueObject(),*docs=xrtValueArray(),*edits=xrtValueArray();int ok=root && docs && edits && xrtValueArrayAppendNew(edits,textEdit(0,0,1,replacement));xvalue* row=ok?documentEdit(uri,hasVersion,version,edits):NULL;if(ok)edits=NULL;ok=ok && row && xrtValueArrayAppendNew(docs,row);if(ok){ok=putEdit(root,"documentChanges",docs);docs=NULL;}xrtValueRelease(edits);xrtValueRelease(docs);if(!ok){xrtValueRelease(root);root=NULL;}return root;}
static inline uint64_t editBlocks(void){
#ifdef XLS_COMPILER_XRT
 xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;
#else
 return 0; /* Historical GUI runtime has no memory-statistics API. */
#endif
}
#endif
