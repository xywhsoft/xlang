# 宿主入口和退役回调的无分配执行登记

日期：2026-10-10。main、初始化、checked 清理和退役准备的生成边界
现在显式登记执行活动，补上裸 C 调用绕过宿主入口记录的缺口。登记
不申请代码引用，关闭准入和 OOM 不会因此跳过已有清理程序。
双端真实 TCC、生成 C 及 Linux 原生 ASan/UBSan 验证通过；完整模块
读取证书和 JSON/XLON 新恢复入口仍未接通。

## 执行活动与代码生命分开

新增私有 resident `XL6_CodeActivityV1`，使用原生调用栈上的记录，
复制已绑定 CodeOwner 的四字段身份，不把扩展尺寸当作本地完整图布局。
Enter/Leave 只在短 XRT mutation 内连接或摘除记录，使用既有注册表锁，
不分配、不调用 acquire/release 或生成代码，也不改变诊断。

记录要求同线程、原地址和嵌套顺序；活记录不可复制。未绑定 owner
表示代码由外部保证常驻，不建立记录。登记不是引用，不增加拥有图
计数，不让 `AcquireScoped` 继承一个不存在的 lease，不重开关闭准入。
真正持有 lease 的普通导出、闭包等仍使用既有 CodeScope。

`xl6CodeOwnerScopeActive/Foreign` 在 Freeze 下同时读取两种登记。
当前同步调用可识别暂停的本线程活动，外线程活动仍阻止模块图读取。
读取证书缺失的真实模块，即使有登记也继续拒绝其 owning slots 检查。

调用者必须通过 handle、宿主入口或独立 lease 保留代码寿命。消费最后
handle 后，独立 lease 的最后释放仍须在生成函数返回后由 resident
调用者完成；裸 GetSymbol 函数地址没有获得自动保活能力。

## XLIR 和生成边界

`ModuleAbi.hostExecutionAbi` 的 lowered 值必须为
`XL6_IR_MODULE_HOST_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1`，typed/无
初始化片段使用 NONE。验证器按阶段和实际初始化函数独立重算，文本
及 JSON dump 保留合同；缺失和未知值在 C/头输出前拒绝。

Cgen 将已有 checked 程序留在私有函数中，公开包装只执行登记、调用、
摘除和返回，语义清理仍由 XLIR CFG 决定。覆盖实际和合成 checked main、
init、checked unit，以及退役合同的 prepare/preflight 和 V4 step。
void/legacy 包装委托 checked 入口，不重复建立一套错误或清理算法。

声明化依赖访客只在实际发出宿主边界时登记两个 resident 符号及所需
组件；合成入口片段不默认强制链接这些符号。模块 ABI 不是读取证书，
不会让 Cgen 输出 `xl_script_module_ownership_read_v1`。

checked main/unit 的布尔返回值是公共主状态。结束其私有清理作用域
不等于向外层 ForeignCall 发布失败事件；宿主须消费该布尔值，不能
仅用错误指针或外层 scope 的状态判断成功。本批保持这个既有合同。

## 冻结源码和批量验证

生产输入来自独立 Git index 快照，完整重建 Windows 251 个、Linux
250 个编译器/API/runtime 单元。发布 DLL/SO 只作为真实 TCC 工具输入，
不代替当前源码的标准库提供者，也不称为新发布产物。

Windows/Linux 的 O0/O2 原生与生成 C 四个普通 lane，以及 Linux 所选
原生闭包的 ASan/UBSan lane，共执行 1,000 组嵌套登记，验证零 XRT
分配、诊断保持、关闭后的非能力语义和全新原生 OS 线程上的外线程检测。
插装 lane 的 XRT、code lease、账本和夹具四个实际单元全部重新编译，
不链接未插装运行时对象或发布库。

真实 TCC 双端 O0/O2 共执行 32 个初始化/main/析构向量、20 次连续
主入口 OOM 前缀尝试、4 次外线程 main，以及 8 次关闭后的 OOM 清理。
每次入口登记的观察器始终调用真实实现，核对分配计数和失败标志不变，
不替换注册表或宿主状态机。完整宿主运行回到测试模块创建前的精确
TCC 工具缓存地址/大小基线，不允许任意残留。

关闭清理中的对象 lease 释放可能触发既有宿主退役图准备并尝试分配；
这是清理后续流程，不是执行登记分配。真实 TCC 验证清理在该 OOM 下
仍完成并只析构一次；静态生成 C 另验证其简单清理整体零分配。

八次静态生成 C 执行覆盖实际/合成主入口、初始化、析构及 V2 prepare；
V3/V4 生成边界已修改，但本专项不宣称这两种退役服务的新增运行验收。
四个模块合同损坏在两次生成和两平台中分别拒绝，入口依赖选择准确。
普通导出回归重新执行 16,800 案例、4,400 次真实分配失败；三个既有
恢复语言程序的 C O0/O2、真实 TCC JIT 及 IR 回归另有 66 个驱动阶段。

本批三个双端门共 170 个驱动阶段。原生宿主严格 C11 Wall/Wextra/Werror，
无警告豁免；生成 C 和旧编译器夹具保留报告列出的既有 unused 等豁免。
O0/O2 指宿主及生成 C 单元，链接的编译器/runtime 为 O0；ASan/UBSan
只覆盖所选原生登记闭包，不覆盖完整编译器、TCC JIT 或所有标准库。

登记报告位于双端 `stdlib53-b162-activity-*-final6/results.json`；
导出和语言回归分别为 `stdlib53-b162-export-*-final1`、
`stdlib53-b162-language-*-final1`。Windows 证据在 `E:/xlang-validation`，
Linux 运行证据在 `/var/tmp/xlang-stdlib53-*`；双端最终 index 复核报告
保存在 `E:/xlang-validation/stdlib53-b162-index-*-final1.json`。

## 后续恢复迁移

其余生成描述符和原生回调仍须完成逐项执行证明，再发出完整读取证书、
绑定宿主读取能力并生成声明 clone thunk，才能切换实际 JSON/XLON 恢复。
其他已批准资源策略、语言载体、冷路径性能与 Windows/Linux JIT/AOT
产品总门继续。本批未新增公开语言函数，不标记长期目标完成。
CRT 无未决策，19 个 XRT 未决策组不变，对象池最后。
