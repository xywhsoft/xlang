# 原生 C 回调的无分配执行登记

日期：2026-10-10。无捕获的原生 C 回调现在按 XLIR 执行合同登记实际
语言执行。C 保存回调地址并在原语言调用返回后再次调用时，宿主能够
识别其当前线程或外线程活动；登记不分配、不申请代码引用，也不重开
关闭准入。Windows 和 Linux 的真实 TCC、生成 C 和相邻回归通过。
完整模块读取证书与 JSON/XLON 新恢复入口仍待接通。

## 保存回调后的执行边界

旧 raw thunk 直接调用语言 closure invoke，没有 closure userdata 所携
lease 的 CodeScope。C 保存地址后，可在原宿主入口结束或其他原生线程
上调用，原入口的 activeCalls 已为零。仅跟踪普通导出和 main 不能覆盖
这段语言执行。

生成 raw thunk 现在在调用前建立栈上的 `XL6_CodeActivityV1`，返回前
摘除，复用既有 resident 登记。checked 失败路径在原有
`xl6ForeignCallbackFail` 之后摘除；no-raise 成功路径同样登记，不加入
分配或错误桥接。违反 no-raise 的既有 abort 合同不变。错误算法、语言
CFG 和参数转换没有另建实现。

执行登记与代码生命仍分离。原生调用者须用模块 handle 或独立 caller
lease 保留代码；裸回调地址不拥有代码。关闭模块后，已保活的回调仍可
登记执行，但不会获得新的准入能力。独立最后 lease 必须在生成函数
返回后由 resident 调用者释放，不能在已卸载的回调代码中继续返回。

## XLIR 合同和按需链接

`ClosureTarget.rawExecutionAbi` 在 typed 阶段为 NONE；lowered 有
raw entry 的目标必须为
`XL6_IR_CLOSURE_RAW_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1`，其模块
必须 bind owner before init。匿名函数与具名适配器均由 lowering
确定，验证器独立按实际 raw signature 和错误 ABI 重算。文本和 JSON
dump 保留合同，缺失或未知值在 C 与头文件输出前拒绝。

依赖访客只对实际可达 raw entry 增加 Enter/Leave 两个 resident 符号
及 memory、atomic、thread 组件，不根据源码名字或生成文本猜测。仅有
普通 closure 或 erased callback 参数不会因此强制链接；没有 main
包装的 C fragment 仍按实际 raw entry 登记依赖。

登记不等于完整读取证书。本批真实 TCC 模块仍不输出
`xl_script_module_ownership_read_v1`，活动或静止时均不能据此绕过缺失
证书的 owning slots 读取限制。

## 双端批量验收

生产输入来自独立 Git index 快照，Windows 251 个、Linux 250 个
编译器、API 和 runtime 单元完整重建。发布 DLL/SO 只提供真实 TCC
工具输入，不代替本批源码的标准库实现，也不是新发布产物。

12 个保存回调覆盖整数、布尔、实数、指针、void 和只读 C 字符串参数，
分别验证 direct no-raise 与 checked 错误 ABI。每次调用核对真实
Enter/Leave 次数和 resident 活动注册表；链接观察器始终执行真实
实现，并核对登记前后的分配、释放、realloc 和失败标志不变。

双端 O0/O2 的真实 TCC 和静态生成 C 八次执行，共验证 192 个返回值与
错误向量、144 次完整诊断分配失败前缀尝试、48 次缺失诊断的 OOM 失败、
96 次关闭前后零分配 no-raise 调用，以及 8 次原生外线程回调。失败
状态不依赖诊断分配成功；普通缺失诊断沿用原有 fallback message，
fallback 的分配失败保持 `ok=false`、空诊断和完整摘除。

真实 TCC 在独立 caller lease 保活但没有外层 CodeScope 的条件下关闭
模块并再次调用回调，证明登记来自回调自身。最后 lease 从 resident
调用者释放后，模块清理和代码卸载完成；完整运行回到精确 TCC 工具缓存
地址与大小的堆基线，检查分配平衡及无 invalid/double free 或 UAF。
静态生成 C 检查关闭准入后没有新增 acquire，清理后引用和堆均平衡。

既有具名 raw callback 的第一错误保持、替换/清除、保存指针和混合
签名继续通过真实 API/TCC 执行，双端 O0/O2 共 36 个原有向量。24 个
缺失或未知 IR 合同变异在两平台分别拒绝，C 及头文件不输出部分内容，
重复生成保持确定性。

同一个严格原生宿主分别链接修复前和修复后的完整源码编译器。双端
修复前均在首次 direct 保存回调调用时触发缺少 Enter 的断言，修复后
完成所有向量；两次预期失败保留为回归检出证据，而非验收失败。

相邻门复测宿主执行登记、普通导出事务和三个既有恢复语言程序，旧导出
事务的 16,800 个案例和 4,400 次真实分配失败通过。Linux 对所选
resident 登记闭包的四个实际原生单元重新插装，ASan/UBSan 通过。
该插装不覆盖完整编译器、TCC JIT 或本批全部 raw thunk。

五项双端门共 214 个驱动阶段，其中 2 个是预期断言失败。原生宿主
使用严格 C11、Wall/Wextra/Werror，无警告豁免；生成 C 和旧编译器
夹具保留报告中的既有警告豁免。O0/O2 指宿主与生成 C 单元，链接的
当前源码编译器/runtime 为 O0，不代表全产品 O2 或全 53 模块验收。

## 证据和恢复迁移边界

Windows 证据位于 `E:/xlang-validation/stdlib53-b163-*`：raw 专项为
`raw-win-dev3`，前后对照为 `red-green-win-final1`，相邻门为
`activity-win-final1`、`export-win-final1`、`language-win-final1`。
Linux 同名目录位于 `/var/tmp/xlang-stdlib53-b163-*`，平台名为 linux。
每个门均以 `results.json` 记录命令、输入、产物 hash 和范围；最终
双端 index 复核为 `E:/xlang-validation/stdlib53-b163-index-*-final3.json`。
后于源码冻结的测试修正显式列入复核，不能静默以旧夹具代替当前测试。

真实 JSON/XLON 恢复还须完成其余生成描述符和原生写入入口的执行证明，
发出完整读取证书、绑定宿主读取边界并生成声明 clone thunk。外部共享
默认子对象只按声明的 clone 能力隔离，否则 checked 拒绝；不修改原
共享对象。其他已批准资源策略、语言载体、冷路径性能和双端 JIT/AOT
产品总门继续。本批没有新增公开 API，不标记长期目标完成。
CRT 无未决策，19 个 XRT 未决策组不变，对象池最后。
