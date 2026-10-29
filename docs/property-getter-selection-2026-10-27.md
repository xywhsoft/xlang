# 属性 getter 的语义归属与统一 IR 收口

本批修复同名零参数／索引 getter 共存时，`player.hp[3]` 无法进入统一 IR
的问题。`cgen_class_property_get_let.xl` 与 `oop_property_get_let.xl` 已恢复。
不新增标准库 API，不改变赋值顺序，也不为 Cgen 添加特殊生成路径。

## 根因与修复

旧类型查询会把索引 getter 绑定到内部 MEMBER，而该节点可能已经缓存了
零参数 getter 的返回类型。IR 构建器随后依靠“子节点没有类型”的条件，
把 getter 目标搬到 INDEX；有同名零参数 getter 时，这个补偿失效，最终
出现 CallSite 参数顺序错误。

现在类型查询只返回克隆类型，不绑定调用或写入实参所有权。正式语义检查
在完整操作上绑定 getter，覆盖普通、索引、协议和可空入口。索引 getter
的 MEMBER 是无类型、无调用目标的 selector；零参数 getter 返回容器时，
MEMBER 则保留其独立调用和类型，后续 INDEX 执行容器读取。

IR 已移除上述搬运逻辑。构建与验证都要求索引 selector 无调用目标；协议
索引入口同样遵循该合同。Lowering、驻留 runtime、Cgen 的生产代码均未
增加分支或补丁。既有赋值 RHS 优先、复合赋值接收者／键／GET／RHS／SET
顺序、单次求值与清理合同保留。

## 测试期望的独立对照

两份既有属性事务测试仍假定基类字符串 getter／setter 是 no-raise，固定
checked 调用数也已过时。使用本批修改前、修改后的源码编译器分别运行
保留所有逐项 mutation 的诊断副本，确认两边均为：

| 测试 | accessor 调用／checked 调用 | 共享 GET／SET 对 | 原有 mutation 总数 |
|---|---:|---:|---:|
| property assignment transaction | 29／29 | 9 | 75 |
| property update | 43／43 | 14 | 192 |

两边生成的 C 均逐字节一致。基类拥有字符串的返回／赋值也可能分配，因此
应保留 MAY_RAISE；本批没有放宽 error edge、接收者暂存、共享操作数、
exit plan 或槽参数校验。更新的是固定计数和对字符串基类效果的旧期望。

另增真实标量基类／抛错 override，验证两个 no-raise 基类方法仍使用槽的
joined MAY_RAISE 合同。新测试同时检查泛型、协议、内嵌 NUL、零参数容器
getter 与同名索引 getter 的区分：五个语义 selector、三个 class／两个
protocol 索引 CallSite、两个标量混合效果槽，32 个错误变异全部拒绝。

## 固定工作线程下的确定性测试

旧 C host 在取得异步输出 Future 后就替换共享状态，隐含了“函数已经执行
到 await”的时序假设。固定线程＋就绪队列不承诺该假设。新 host 独立保留
源 Future，通过已有 `xrtFutureDebugSnapshot.PendingWatches` 确认真实的
pending 源订阅；getter 提前失败则等待输出完成。随后才替换接收者和键。
保留的 Future 引用在每次操作后释放，不读取私有 frame 布局。

Future 完成也不代表物理 release 尾段已经结束。分配快照前，host 在序列化
生产者的条件下等待现有 `xl6ContinuationExecutorIsIdle`，包含物理清理和
code-owner 尾段；不关闭工作池、不改变生产调度策略、不以固定延迟充当
完成证明。两份原生事务测试均保留 100 轮及预热，验证 2400 次 pending 源
等待、800 次 await 前 getter 失败，以及错误、消费型键、泛型和完整清理。

## 验证结果

- Windows／Linux 各 60 项全部通过：六组严格 IR 合同，三组 C host 的
  GCC O0／O2、确定性 C／头文件，四个语言程序的 JIT／AOT／GCC O0／O2。
- 既有 indexed accessor：12 个 getter、18 个具体泛型成员 owner、144 个
  拒绝变异；nullable getter：18 个站点、86 个拒绝变异。
- 原生工厂 getter：两个工厂、七个站点、12 个拥有回滚、61 个严格拒绝。
  地址仅做固定数字地址解析，不使用 DNS、socket、Engine 或网络通信。
- 完整字符串专项 364 项通过，涵盖文本 ABI、真实分配失败、插值、原生
  容器显示、模块清理、前端 fuzz 与 C／xlang 示例一致性。
- 同内容哈希的旧 536 个源码样本：502 → 504 成功，恢复上述两个样本；
  之前成功的 502 份 C 全部字节一致。其余 32 个拒绝项保持不变，其中含
  负例和过时示例。514 接纳下限保持，尚未通过该全量门槛。

本批在一致的 Windows 1295／Linux 1294 单元源码编译器上重编语义与 IR
两个单元，IR 结构布局未变；编译器相邻源码包同步相同内容。TCC 核心仍为
固定的真实依赖。这是源码编译器与普通语言／ABI 验收，不等同于新发布
DLL／VFS 或全部 74 个参数化 guard 已通过。

## 复跑与证据

```text
python tools/check_xlir_property_selection.py
  --root <一致源码树>
  --compiler-directory <完整源码编译器目录>
  --work-directory <全新证据目录>
  [--cc <gcc> --ar <ar>]
  [--native-root <Linux 固定真实运行库根目录>]
```

脚本保留独立命令的全部结果，C host 仅在私有副本里重定位两条生成文件的
include 路径。早期路径／文件模式、异常名称、旧断言及未同步采样失败均
保留在独立目录，未删除 fixture。

证据根：Windows `E:/xlang-validation/five-groups-20261027/p7-shipping`；
Linux `/home/ubuntu/xlang-validation/20261027-p7-shipping`。

| 相对路径 | 范围 |
|---|---|
| `property-cohort-r4/build.json` | 语义／IR 两单元及相邻源码包 |
| `property-cohort-r14/build.json` | 最终测试的不可变部署 |
| `property-check-r15/results.json` | 每平台 60 项完整结果 |
| `property-strings-r16/evidence/results.json` | 完整字符串专项 |
| `property-corpus-r5/results.json` | 固定 536 个输入及恢复证据 |
| `property-contract-audit-r7/results.json` | 修改前／后旧测试计数与 C 一致性 |
| `property-proof-r17.json` | 独立重算源码、对象、日志与当前产物哈希 |

本轮不包含公网探测、扫描、攻击模拟或网络安全专项测试。长期任务继续，
其他语言拒绝项和全量参数化验收尚待收口。
