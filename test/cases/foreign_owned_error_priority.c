#include "foreign_owned_error_priority.h"
#include "foreign_output_snapshots.c"
static int priorityReleases;
static void later(void* user)
{(void)user;++priorityReleases;xrtSetErrorTake(xrtErrorCreate(XERR_STATE,"priority.cleanup",91,"cleanup later"));}
int priorityProduce(int status,xvalue** out)
{
 *out=NULL;
 if(status!=2){
  *out=xrtValueObject();if(!*out)return -8;
  if(!xrtValueObjectLifetimeBindOwned(*out,NULL,NULL,later))return -8;
 }
 xrtSetErrorTake(xrtErrorCreate(XERR_RANGE,"priority.producer",73,"producer first"));
 return status==1?-9:0;
}
int priorityProduceScoped(OutputValueRoot* root,xvalue** out)
{
 assert(root&&root->active);root->poison=99;*out=NULL;
 xrtSetErrorTake(xrtErrorCreate(XERR_RANGE,"priority.producer",73,"producer before scope cleanup"));return 0;
}
int priorityCode(xerror* error){return xrtErrorCode(error);}
int priorityReleaseCount(void){return priorityReleases;}
