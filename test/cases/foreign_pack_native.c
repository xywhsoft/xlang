/* Descriptor consumers are real native entry points, without forwarding adapters. */
#include "foreign_pack.h"
#include <__xl_runtime/xl6_foreign_view.h>
#include <assert.h>
#include <string.h>
static int live,calls;
static int evaluate(PackRoot* root,const PackDesc* d,PackCallback callback,void* user,int* output);
int pack_create(int value,PackRoot** output)
{*output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;(*output)->refs=1;(*output)->value=value;(*output)->evaluate=evaluate;++live;return 0;}
void pack_drop(PackRoot* root)
{if(!root)return;assert(root->refs>0);if(!--root->refs){--live;xrtFree(root);}}
int pack_value(PackRoot* root){assert(root && root->refs>0);return root->value;}
unsigned pack_seen(PackRoot* root){assert(root && root->refs>0);return root->seen;}
static int inspect(const PackDesc* d,unsigned* seen)
{
 assert(d && d->size==sizeof(*d));unsigned bits=256;
 assert(d->text && d->textSize>=0 && (!d->contextSize || d->context));
 if(!d->context)bits|=1;else if(!d->contextSize)bits|=2;
 if(!d->language)bits|=4;else if(!d->language[0])bits|=8;
 if(d->textSize==5 && !memcmp(d->text,"a\0\xe4\xbd\xa0",5))bits|=16;
 if(d->font){assert(d->font->refs>0);bits|=64;}
 if(d->dashCount==2 && d->dashes && d->dashes[0]==1.0f && d->dashes[1]==2.0f)bits|=128;
 if(seen)*seen=bits;
 return d->mode*10+d->factor+d->textSize+(d->font ? d->font->value:0)+d->dashCount;
}
static int evaluate(PackRoot* root,const PackDesc* d,PackCallback callback,void* user,int* output)
{
 assert(root && root->refs>0 && output);++calls;
 int before=inspect(d,&root->seen);
 if(callback){int result=callback(user);if(result<0)return result;}
 assert(inspect(d,NULL)==before);*output=before;return 0;
}
int pack_free(const PackDesc* d,int* output)
{++calls;*output=inspect(d,NULL);return 0;}
void pack_void(int* output,const PackDesc* d)
{++calls;*output=inspect(d,NULL);}
int pack_make(const PackDesc* d,PackRoot** output)
{++calls;return pack_create(inspect(d,NULL),output);}
int pack_two(const PackSmall* a,int* x,const PackSmall* b,int* y)
{++calls;*x=a->value;*y=b->value;return 0;}
int pack_calls(void){return calls;}
int pack_live(void){return live;}
void pack_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
void pack_reset(void){assert(xrtMemDebugFailAfter(-1));}
static bool destroy(void* raw){pack_drop(raw);return true;}
const XL6_ForeignViewDescriptorV1 packPolicy={sizeof(packPolicy),XL6_FOREIGN_VIEW_V1_TAG,"pack.root",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL};
