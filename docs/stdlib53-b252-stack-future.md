# Future 元素的 Stack 合同与验收

2026-10-13。`Stack<future<T>>` 和 `Stack<future<T>?>` 已接通原生
Future 槽。创建、push、peek、pop、try 系列、clone、equals、reserve、
clear、trim 和 Array 批量交换通过双平台执行验证。本批复用现有运行时，
不增加标准库名称、驻留组件或导出，不修改 XRT 和 xruntime。

## 统一存储合同

Stack 的元素类型、物理槽、拥有传输和 codec 来自其已验证的 Sequence
配方。声明签名、降低、指令验证和 Cgen 使用同一个 owner 局部合同，
不再用通用参数 ABI 推断 Stack 的物理存储。

Future 的物理槽为 `xfuture*`，描述为 `xrtTypeFuture()`，传输为
Future 引用取得与释放。普通通用参数 ABI 仍不接受 Future，不能因为
Stack 接通而隐式放宽其他资源的元素域。函数值需要不同的 boxed codec，
仍被本批的直接存储合同拒绝。

Cgen 只选择配方的原生描述和既有运行时入口。单项读取取得独立 Future
引用，弹出移动已有引用；批量读写复用 Stack 与 Array 的精确物理配方。
批量结果沿用来源的准入策略，原栈删除前完成结果拥有者与载荷准备。

`tryPop`、`tryPeek` 和 `tryPeekAt` 的 found 位独立于可空 Future 载荷。
成功取得 null 与没有元素保持不同结果。原生复制失败返回 checked 错误，
不改变原栈或发布未拥有的结果。

## 双平台验证

Windows 323、Linux 322 个完整源码编译单元通过。专项分别完成 90、
121 个阶段，包含严格独立 C O0／O2、实际 JIT、module 和 AOT。
每个优化等级执行 100 次语言模块初始化与清理，以及 200 次原生生命周期。
取出的引用在来源销毁后仍有效，内存数量、字节和分配释放计数平衡。

每个优化等级运行两种 Future 域的 50 次完整分配故障前缀，包含成功
终止点。创建、clone、reserve、批量 push、peek 和 pop 的失败路径通过；
失败后原栈的数据指针、分配、数量和容量保持不变。单项 peek 的实际
复制入口失败注入核对 found=false、空结果及原栈保留。故障注入只覆盖
同步原生调用线程，不代表 executor 线程分配失败已认证。

普通和可空域各验证一组 Stack 与 Future 的真实循环所有权图。未授权
准入策略被拒绝；授权后的图计划可提交、完成并释放循环，弱引用到期。
这两组图验证不代表图遍历 OOM 已封板。

两种 Future Stack 域和 37 个调用的 82 项 IR 篡改被验证器、C／头文件
发射与原生计划拒绝。两个实际错误实现分别恢复旧通用 ABI 判定、允许
必选 Future 槽存 null，均被拒绝。此前普通原生键 Stack、可空原生键
Stack 和 Future Array 在每端完成 9 个 JIT／module／AOT 执行回归。

Linux ASan／UBSan 覆盖生成程序、全部 22 个所选供应方、原生与语言
宿主。原生单元另显式链接 Array 核心和对象图计划两个供应方并插桩；
这两个供应方不计入语言程序的按需裁剪结果。

三组声明与严格 C 批量检测每端 295 夹具、589 阶段通过。驻留目录仍为
284 组件、1458 个精确导出，13 个拒绝对照通过。证据位于
`E:/xlang-validation/stdlib53-b252-*` 和 Linux 的
`/var/tmp/xlang-stdlib53-b252-*`，最终证书核对编译输入、日志、产物和
所选源码的一致性。中途失败的构建或测试尝试不计入最终验收。

## 剩余工作

函数值 Stack、其余容器组合、WaitQueue、独立 borrowed SSA、工作线程
与图 OOM，以及三组发布总门继续。本批不认证 shipping DLL、VFS 或
发布 SDK。CRT 0、XRT 19 组未决策不扩展，对象池仍排在最后。
