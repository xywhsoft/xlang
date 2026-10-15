# 原生 Context 和 Buffer 的驻留拥有合同

`xlang.Context` 和 `Buffer` 的原生 payload 已改为声明式驻留策略，
不再保存生成模块中的 Drop 包装。上一轮嵌套 SDK 两项独立 AOT
在主逻辑输出 PASS 后退出失败的问题，已在两端真实产品中复测
通过。本批未增加公开函数，也未将所有原生指针统一视为叶节点。

## 两种资源的实际拥有范围

`XL_Context` 只拥有配置、文本、路径和系统库句柄。符号表中的 C
函数地址是借用；编译单元、运行模块和传入参数由各次调用的栈
持有，而非 Context 的拥有字段。原有 activeCalls 租约会将并发
销毁推迟至最后一次调用返回。这个合同不适用于拥有模块的
XlangHandler，也不适用于带编译单元的 XLC_Context。

XRT 的 `xbuffer` 只有 Data、Size、Capacity 三个字段，拥有原始
字节，不保留语言对象或回调。策略适用于该真实字节资源，不是
“任何容器均无子边”的通用声明。审计守门覆盖 Context、其三个
原生辅助记录以及 xbuffer 的字段布局，新增拥有字段必须重新审计。

两种策略都使用实际驻留的 Ops 与 Trace 身份。复制出的相同 Ops
或未知 Trace 不能取得准入，未知图回调不会被执行。拥有转移在
分配失败时仍消费原槽并只销毁一次资源，保留首个错误。

## 声明和编译部署

原生析构声明中的精确策略名同时进入 typed 字段合同与 lowered
调用目标。Cgen 消费同一元数据，复用已有 NativeHandleTake 路径，
没有针对 Context 或 Buffer 增添后端语义分支。擦除或替换任一
字段／调用目标策略都会在输出任何 C 之前被拒绝。

驻留目录增加两个独立领域，按实际组件闭包选择。普通标量计划
不会因此引入嵌套宿主；Buffer 也不依赖 Context 策略。领域 getter
只通过唯一的领域登记展开，不再重复列入符号表。

Context 的销毁依赖版本化的嵌套宿主 ABI，而不是未声明的外部
符号。该需求由组件声明，经闭包派生并保留在拥有计划中；独立
编译检查确实选择了嵌入式 xlang 宿主。组件审计还核对实际宿主
对象中的定义，缺少 ABI 声明、未知 ABI 位或缺少实际定义均拒绝。
这不是对未解析符号的前缀或名称放行。

## 专项和产品验收

专项覆盖 O0、O2 的严格 C、精确 IR 反例、纯标量裁剪、真实
Context 调用与并发延迟销毁，以及 Context／Buffer 拥有转移的
完整 OOM 前缀。每条转移各扫描两个前缀，首个未触发点为 1，
内存计数平衡。Linux 的选定 API、通用 handle、Context 策略和
Buffer 提供方使用 ASan、UBSan；不代表全部提供方均已加插桩。

最新完整源码构建为 Windows 394、Linux 393 个翻译单元。两端
Context／Buffer 专项分别完成 10、18 个阶段；既有拥有目录和
JIT 图专项各 110 个阶段通过。驻留目录现在覆盖 355 个组件、
1589 个精确导出和 126025 对依赖关系，并含 26 项拒绝反例。
目录可分别从实际宿主对象和实际 DLL／SO 核对宿主定义。

普通构建重建了 TCC、XRT、CLI、运行库、编译库及 VFS 镜像。
原有嵌套 SDK、字符串边界、嵌套 AOT 和 Future 元数据四个用例
在源目录之外运行，包含严格 C、独立 AOT 与搬离目录后的执行。
这些产品专项与完整三组清单分别验证，不等于所有公开函数或
所有原生资源形态均已证明可独立退役。

三组普通夹具两端各枚举 311 项：303 项执行通过、一项按预期
语义拒绝、六项需要专用宿主、一项需要相应平台条件。Windows
的历史 DST 条件用例和 Linux 的旧平台进程用例仍显式保留，
不计为已执行。容器图、clear、线程与锁恢复由独立宿主专项
验证，不能据普通夹具计数宣称全部公开函数覆盖。

数值转换的十项既有断言夹具也在最新两端产品中各完成 61 个
阶段，覆盖 CLI、公开模块调用与独立 AOT。静态／动态显式
构造保持同一安全转换规则，不因本批拥有策略改变为 C 裸转换。

## 可达原生入口的后续审计

新增扫描消费验证后的 lowered IR，按照函数及异步机器可达块
枚举实际 native handle 转移；不从生成 C 或函数名前缀猜准入。
三组 311 个用例加嵌套 SDK，共 312 项完成扫描，包含一项原有
预期语义拒绝。仍有 31 个可达析构入口缺少驻留拥有策略：

- 时间：DateTime。
- Pattern：Pattern、Builder、Edit、Key、Match、Options。
- 进程：PipelineResult、ProcessPipeline、ProcessResult、RunOptions、Stage。
- 正则：Regex、Iterator、Match、Replacer、Set、Split、SplitPart。
- 信号：Event、Queue、TaskGroup。
- 文本：TextIterator、UnicodeValidator。
- 现有用例带入的基础依赖：文件、IO Reader／Writer、随机生成器、
  VFS、VFS Memory、VFS Mount。

精确 C 名称、使用它们的用例及策略状态由
`tools/check_stdlib_native_handle_inventory.py` 的 results.json 输出。
“未声明策略”是需要实际存储与生命周期审计的缺口，不等于上述
每个函数已经实测失败，也不授权将它们全部注册为叶资源。尤其
信号队列、任务组及 Handler 等须核对是否持有语言回调或模块。
后续按资源族批量审计、补合同并统一验证，不降低原目标的范围。

## 仍未封板

最新 Linux 产品汇总门完成，Windows 产品汇总门仍失败：
`stdlib_stack_clear_error_continuation` 搬离目录后的独立 AOT
报 RuntimeError。最新二进制与上一批 Windows 二进制各重复
20 次均失败；观察用 sidecar 的输出说明语言主体可以执行完毕
后才失败。该观察不是修复或验收，不修改原断言，也不凭先前
偶尔通过关闭问题。退役清理路径需继续定位，根因尚未确定。
其余产品专项通过，包括本批修复的嵌套 SDK、容器图、线程、
锁恢复与原有独立 AOT 图析构断言。

31 个入口的审计、全局 exit 的完整清理实现、Windows 终端尺寸
间歇失败、上述 clear 退役问题及完整发布验收继续。CRT 未决策为零，XRT 其余十九组
未决策不变；对象池仍最后处理。
