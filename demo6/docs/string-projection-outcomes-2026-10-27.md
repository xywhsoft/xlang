# 字符串投影的单次结果检查

字符串字段复合赋值会经过通用运行时元数据发射和投影读取两个入口。
前者已经立即检查分配结果，后者又为同一 SSA 值建立一次 ERROR_TEST。
第二次检查已不紧邻生产者，因此 verifier 正确拒绝了合法的语言程序。

降低阶段现在复用已有 CFG 表达的结果：只有生产者的紧邻 ERROR_TEST 检查了
同一结果、其成功目标正是当前 block，而且当前 block 仍是未执行任何指令的
新鲜成功延续时，才不再重复发射。已有的失败清理及传播路径完整保留。
一旦成功延续已经发射工作，就不能据此跳过检查。没有缓存、额外 IR 字段、
运行时接口、操作码白名单或 Cgen 推断；verifier 的立即检查规则保持不变。

新增语言用例包含保留 NUL／Unicode 的字符串字段、复合赋值，以及保有和临时
接收者读取。在 128 个真实分配失败注入点上，失败必须进入 MemoryError，
赋值失败不改变原目标，成功结果拥有完整独立内容。原有字符串拥有权用例也
作为正例。IR 合同要求每个相关生产者恰有一个紧邻的结果检查；破坏检查的
输入时，IR、C 和头文件必须全部拒绝。

| 检查 | 结果 |
| --- | --- |
| Windows 语言 JIT、独立 AOT、GCC O0/O2 | 新用例每配置 128 项，通过 |
| Windows IR 合同 O0/O2 | 新用例 3 个、原拥有权用例 2 个单次检查，通过 |
| Linux 常规 JIT、GCC O0/O2、IR 合同 | 30 项完整检查，通过；含原有字符串分配失败及两个容器案例 |
| 原有全部 536 源 | 502 成功，1 项恢复，499 个既有成功 C 不变，34 项既有拒绝不变 |
| 另两个既有成功容器案例 | 移除重复分支，错误测试数从 183→174、93→92；两平台运行通过 |
| 两个容器案例 Windows 独立 AOT | 通过 |

两个容器案例的 C 分别由 1,395,405→1,358,958 字节、199,076→197,923 字节。
这些是特定生成 C 的体积变化，不是发布编译器 DLL 的体积或性能测量。

正式字符串 guard 接入了新增合同和 JIT/AOT 用例，并迁移已过时的测试底座：
Split／Lines／Chars 精确检查 V2 与 NULL 结果错误合同；其他入口保持 V1 与
线程错误合同。源码链接使用已核验的完整编译器／运行时对象组及实际 TCC 核心
依赖；GCC 使用现有响应文件助手，保留原来的 120 秒阶段期限。测试宿主的
依赖组不用于宣称发布裁剪或零开销。

整门仍然失败，不能记为通过。新增六个阶段、十项错误 ABI 声明拒绝、原生
join／字符串分配失败检查和四线程 2,000 次 join 已完成，但语言案例在
`string(native Dict)` 处报“Value type has no XLON representation”。最小复现：

```text
array nested = [[1, 2], {"answer": 42}]
println(string(nested[0]))  // [1,2]
println(string(nested[1]))  // 当前原生 Dict 文本投影缺口
```

旧、新源码编译器均复现同一失败；该原生 Dict 文本投影是下一项独立核心问题。
没有删除原断言、吞掉错误、跳过原案例，或把未执行的后续检查视作通过。
最小 514 个成功样本门槛保持不变，502 仍不足；74 个参数化 guard 和长期任务
均未完成。

Windows 证据位于 `E:/xlang-validation/five-groups-20261027/p7-shipping/` 的
`projection-outcomes-r3`、`projection-native-r1`、`projection-corpus-r2`、
`projection-collections-r1`、`projection-display-baseline-r1` 和
`projection-string-gate-r8`。早期控制器／fixture／依赖部署失败记录继续保留。
Linux 证据位于 `/home/ubuntu/xlang-validation/20261027-p7-shipping/projection-outcomes-r2`；
使用核验过的既有 1,297 对象及两个精确源替换，并在原生文件系统上建立普通
静态 archive 供链接器选取，不宣称全新重编整个 Linux cohort。

独立 reader 重算 Linux 3,664 项；Windows 的独立 reader 同时保留整门失败和
旧、新编译器的原生 Dict 失败证据。所有检查限于本机常规语义、生命周期和
故障传播，没有网络安全专项测试。
