#include "test/cases/foreign_borrow_variants.h"
#include <assert.h>
static int calls;
int variant_create(BorrowRoot** out){*out=xrtCalloc(1,sizeof(**out));return *out ? 0:-1;}
void variant_destroy(BorrowRoot* root){if(!root)return;root->closing=true;if(!root->depth)xrtFree(root);}
int variant_take(BorrowRoot* owner,BorrowRoot* argument){assert(owner && owner==argument && owner->depth>=2);++calls;return 17;}
int variant_with(BorrowRoot* root,int(*callback)(BorrowRoot*,void*),void* user){assert(root && root->depth && callback);return callback(root,user);}
int variant_calls(void){return calls;}
static bool begin(void* raw,void* storage){BorrowRoot* root=raw;BorrowScope* scope=storage;if(scope->root || root->closing)return false;scope->root=root;++root->depth;return true;}
static bool end(void* storage){BorrowScope* scope=storage;BorrowRoot* root=scope->root;assert(root && root->depth);scope->root=NULL;--root->depth;if(root->closing)variant_destroy(root);return true;}
static bool destroy(void* raw){variant_destroy(raw);return true;}
static bool busy(const void* raw){return ((const BorrowRoot*)raw)->depth!=0;}
const XL6_ForeignViewDescriptorV1 borrowPolicy={sizeof(borrowPolicy),XL6_FOREIGN_VIEW_V1_TAG,"borrow.variants",0,1,NULL,NULL,destroy,sizeof(BorrowScope),_Alignof(BorrowScope),begin,end,NULL,busy};
