#define XRT_MODULE_ALL
#include "src/common/xl6_xrt.h"
#include <assert.h>
#include <stdio.h>
extern int xl_script_module_init(xvalue*);
extern int xl_script_main_checked(xvalue*,int*);
extern int xl_script_module_unit_checked(void);
extern void xl_script_print_uncaught(void);
#include "test/cases/foreign_xge3_binding_managers.h"
static xallocationbudget* managerBudget;
static xallocationscope managerBudgetScope;
static int managerCallbacks;
int binding_manager_budget_begin(void)
{
 assert(!managerBudget && !managerBudgetScope.Budget);
 if(!xrtAllocationBudgetCreate(1,&managerBudget))return -1;
 if(!xrtAllocationBudgetEnter(managerBudget,&managerBudgetScope)){xrtAllocationBudgetFree(managerBudget);managerBudget=NULL;return -2;}
 return 0;
}
int binding_manager_budget_end(void)
{
 assert(managerBudget && xrtAllocationBudgetLeave(&managerBudgetScope));
 xallocationbudgetinfo info={0};info.Size=sizeof(info);assert(xrtAllocationBudgetInfo(managerBudget,&info));
 xrtAllocationBudgetFree(managerBudget);managerBudget=NULL;managerBudgetScope=(xallocationscope){0};
 return info.Rejections>0;
}
void binding_manager_callback_entered(void){++managerCallbacks;}
int binding_manager_callback_count(void){return managerCallbacks;}
int main(void)
{
 int code=-1;
 if(!xl_script_module_init(NULL) || !xl_script_main_checked(NULL,&code)){xl_script_print_uncaught();return 90;}
 assert(!code && xl_script_module_unit_checked());
 puts("actual public Manager declarations, descriptor-pack factories, allocator-OOM, callback failure, invalidation and Context close passed");return 0;
}
