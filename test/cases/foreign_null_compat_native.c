#include "foreign_null_compat.h"
#include <assert.h>
static int calls;
int null_create(NullRoot** out){*out=xrtCalloc(1,sizeof(**out));if(!*out)return -1;(*out)->value=5;(*out)->apply=null_pack;(*out)->paired=null_paired;return 0;}
int null_create_config(const NullDesc* desc,NullRoot** out){++calls;int status=null_create(out);if(status)return status;if(desc){assert(desc->size==sizeof(*desc) && desc->source && !strcmp(desc->text,"packed"));(*out)->value=desc->mode+desc->source->value;}return 0;}
void null_drop(NullRoot* root){xrtFree(root);}
static bool destroy(void* raw){null_drop(raw);return true;}
const XL6_ForeignViewDescriptorV1 nullPolicy={sizeof(nullPolicy),XL6_FOREIGN_VIEW_V1_TAG,"null.compat",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
int null_pod(NullRoot* root,const NullPod* pod,int n,const char* text,int* out){assert(root && text && !strcmp(text,"end"));++calls;*out=root->value+(pod ? pod->value:0)+n;return 0;}
int null_pack(NullRoot* root,const NullDesc* desc,int n,const char* suffix,int* out){assert(root && suffix && !strcmp(suffix,"end"));++calls;*out=root->value+n;if(desc){assert(desc->size==sizeof(*desc) && desc->source==root && !strcmp(desc->text,"packed"));*out+=desc->mode+desc->source->value;}return 0;}
int null_paired(NullRoot* root,const NullDesc* a,const NullNested* b,int n,const char* suffix,int* out){assert(root && b && b->size==sizeof(*b) && b->inner.size==sizeof(b->inner) && b->inner.source==root && !strcmp(b->inner.text,"packed") && suffix && !strcmp(suffix,"end"));++calls;*out=root->value+n+b->inner.mode+b->inner.source->value;if(a){assert(a->size==sizeof(*a) && a->source==root && !strcmp(a->text,"packed"));*out+=a->mode+a->source->value;}return 0;}
int null_free(const NullPod* pod,int n,const char* text,int* out){assert(text && !strcmp(text,"end"));++calls;*out=n+(pod ? pod->value:0);return 0;}
int null_calls(void){return calls;}
