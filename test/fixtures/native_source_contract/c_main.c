#include <stdio.h>

/* 混编时类型和导出函数声明由 xl 自动注入。独立 DLL 消费者则显式包含生成头。 */
#ifdef XL_TEST_DLL_CLIENT
#include "module_api.h"
#else
extern int xl_script_module_unit_checked(void);
#endif

/* 这些接口全部出现在生成头里，独立消费者必须能够链接并使用它们。 */
static int check_scope_contract(void)
{
    xl6_error_scope outer = {0}, cleanup = {0};
    xl6_error_result outcome;
    if (!xl6ErrorScopeBegin(&outer)) return 10;
    if (!xl6ErrorScopeBeginCleanup(&cleanup, false)) return 11;
    xl6ErrorScopeCaptureFailure(&cleanup);
    outcome = xl6ErrorScopeEnd(&cleanup);
    if (outcome.ok || outcome.error != NULL) return 12;
    xl6ErrorScopePropagateFailure(outcome);
    outcome = xl6ErrorScopeEnd(&outer);
    xrtErrorFree(outcome.error);
    if (outcome.ok) return 13;

    if (!xl6ErrorScopeBegin(&outer)) return 14;
    xl6ForeignCallbackFail();
    outcome = xl6ErrorScopeEnd(&outer);
    xrtErrorFree(outcome.error);
    return outcome.ok ? 15 : 0;
}

int main(void)
{
    xl6_foreign_error_scope scope = {0};
    xl6_foreign_error_result outcome;
    int result;

    /* 导出入口负责模块初始化；返回值不能代替跨语言调用的成功状态。 */
    if (!xl6ForeignCallBegin(&scope)) return 2;
    result = xl_native_module_probe();
    outcome = xl6ForeignCallEnd(&scope);
    xrtErrorFree(outcome.error);
    if (!outcome.ok) return 3;
    if (result != 0) return 3;

    /* 跨 DLL 异常也由同一运行库持有；失败后成功调用不能被旧错误污染。 */
    if (!xl6ForeignCallBegin(&scope)) return 5;
    result = xl_native_failure();
    outcome = xl6ForeignCallEnd(&scope);
    if (outcome.ok || outcome.error == NULL || result != 0) return 6;
    xrtErrorFree(outcome.error);

    if (!xl6ForeignCallBegin(&scope)) return 7;
    result = xl_native_module_probe();
    outcome = xl6ForeignCallEnd(&scope);
    xrtErrorFree(outcome.error);
    if (!outcome.ok || result != 0) return 8;
    result = check_scope_contract();
    if (result != 0) return result;

    /* DLL 拥有独立运行库，清理失败不能宣称允许卸载。 */
#ifdef XL_TEST_DLL_CLIENT
    if (!xl_script_runtime_retire_checked()) return 4;
#else
    if (!xl_script_module_unit_checked()) return 4;
#endif
    puts("native source contract ok");
    return 0;
}
