# C 导出函数的运行时执行跟踪

日期：2026-10-10。Cgen 生成的普通导出函数现在通过既有 resident
CodeScope 登记完整执行区间，补上绕过公开宿主调用时的活动检测缺口。
实际 TCC 模块的返回、错误、回滚、跨线程活动和关闭后重入验证通过。
这为共享默认对象的安全恢复提供前置能力，尚未切换 JSON/XLON 入口。

## XLIR 明确声明执行合同

`XL6_IrExport.executionAbi` 必须为
`XL6_IR_EXPORT_EXECUTION_RESIDENT_CODE_SCOPE_V1`，模块必须具备初始化前
绑定 CodeOwner 的合同。lowering 显式填写，验证器拒绝缺失或未知值，
文本和 JSON dump 均保留。Cgen 只按这个合同输出已有运行时调用，
不追溯 AST，不复制执行注册表或内存管理实现。

导出函数在错误事务成功开启后、模块初始化前取得 scoped code lease，
随后进入 CodeScope。参数桥接、函数体、参数清理和失败结果回滚均在
该区间内；成功及失败分支都退出 scope。获取失败保持 checked 失败，
不执行模块初始化或函数体，也不依赖能否分配诊断对象。

已绑定模块的首次外部入口可能分配一个 lease；同线程、同 owner 的
嵌套入口复用已接受的 lease，包括停止新准入后的合法重入。没有绑定
owner 的静态驻留代码不分配 lease。依赖访客显式登记四个运行时符号
及 memory、atomic、thread 组件，不通过扫描生成文本推断或强制全链接。

## 原生调用者仍负责代码生命

`xlModuleGetSymbol` 返回的裸函数地址不是拥有引用。调用者必须在整个
调用期间保留模块 handle，或持有独立的入口 lease。若回调请求销毁
模块并消耗最后 handle，独立 lease 的最后释放必须发生在生成函数
返回后，由 resident 调用者执行；生成代码不能负责最后一次卸载自身。

实际 TCC 夹具使用真实公开 API、Module 和 resident CodeScope，不复制
宿主状态机。关闭后嵌套调用沿用调用者的 pin，生成函数先返回，再由
原生调用者退出 scope，完成真实模块清理和 TCC 代码页卸载。

## 双平台批量验收

冻结 Git index 中的生产源码，重新编译 Windows 251 个、Linux 250 个
编译器/API/runtime 单元。发布目录中的 DLL/SO 仅作为 TCC 工具输入；
标准库原生提供者来自当前源码，不将这些工具文件称为新发布产物。

Windows/Linux 各以 O0/O2 编译并执行原生宿主及生成 C 夹具，合计：

- 真实 TCC 的 36 次返回/错误向量执行、12 次连续 OOM 前缀尝试、
  4 次真实外线程活动和 4 次关闭/重入/卸载检查。
- 静态生成 C 的 24 次返回/错误向量执行，以及嵌套 lease 复用、
  关闭后新入口拒绝、引用与堆分配平衡检查。
- 旧导出事务的 16,800 个案例、4,400 次真实分配失败和
  35,600 次平衡分配；13 个导出的 104 次损坏合同拒绝，均在 C/头输出前。
- 三个既有数据恢复语言程序的双端 typed/lowered IR、确定性 C、
  C O0/O2 和真实 TCC JIT 回归，共 66 个驱动阶段。

真实 TCC 在进程中保留工具路径缓存。完整宿主运行以创建测试模块前的
该缓存地址、大小、分配身份为基线，核对新增分配全部释放；不把任意
单个残留当作通过。每次 OOM 尝试另有独立真实分配账本。

旧事务夹具的释放故障注入改为实际 `xrtFree` 链接边界，仅在精确的
桥接对象身份上触发，始终执行真实释放；原错误与回滚断言未放宽。
重新运行时只复用字节相同的生成夹具文件，所有可执行文件均重新构建。

原生宿主以严格 C11 `-Wall -Wextra -Werror` 编译，无警告豁免；生成 C
与旧编译器夹具保留检查器记录的既有 unused 等警告豁免。O0/O2 指
宿主/生成 C 单元，链接的编译器与 runtime 单元为 O0。本批没有新增
ASan/UBSan、完整发布 AOT 或 53 模块产品总验收的声明。

导出证据为 `E:/xlang-validation/stdlib53-b161-execution-win-dev9/results.json`
及 `/var/tmp/xlang-stdlib53-b161-execution-linux-dev9/results.json`；语言
回归为双端 `stdlib53-b161-language-*-final1/results.json`。
双端 index 最终核对报告为 `E:/xlang-validation/stdlib53-b161-index-*-final1.json`。

## 下一步接通数据恢复

普通导出跟踪并不等于完整模块读取证书。其他入口、外部原生写入和
所有触及 owning slots 的执行仍须逐项认证；当前 Cgen 不输出
`xl_script_module_ownership_read_v1`，真实模块也没有该读取能力。
随后才能绑定独立宿主读取边界、生成声明 clone thunk，并切换实际
JSON/XLON 原生恢复控制器。其他已批准资源策略、语言载体、冷路径性能
及产品总门继续，不将本专项记为长期目标完成。

未新增公开标准库函数。CRT 无未决策，19 个 XRT 未决策组不变，
对象池仍放在最后。
