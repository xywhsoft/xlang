# C 所有权链路：类型化方法绑定与引用容器补齐

本轮对应上一轮审计中的五项功能缺口，以及 native 绑定方法返回字符串的复制。
继续使用现有 string、closure、Error 和容器表示；不增加字符串来源标签，不修改
XRT 分配器合同，也不要求扩展库添加转发函数。

阶段提交：`14a86e588`（nullable 泛型值）、`f11b13dbb`（Error 提取与
丢弃）、`ca5de68d7`（提取前认证及错误保留）、`4baaa612f`（类型化方法绑定）。

## 实现

- 泛型调用已推得 `T=error?` 或 `string?` 时，直接值参数可以接受对应非空值或
  null。语义推断、重载筛选和 lowering 使用同一规则；不放宽 `array<T>` 等
  可变容器的不变性。这使 async Channel 泛型包装与同步调用保持一致。
- Array 的 `pop/take` 支持 Error 与 nullable Error。读取并认证保留引用后才
  提交移除，类型/分配失败不消费元素，已有诊断独立保留。丢弃 Error 返回值
  也执行正确的引用释放。
- 具体类的成员绑定到固定函数签名时，由 Lowered XLIR 确认唯一重载，构造只
  捕获一个接收者的普通 typed closure。非空分支中的可空接收者使用原有流投影；
  泛型实例方法先在依赖闭包阶段具体化，不在 C emitter 或运行时恢复类型。
- 调用复用既有的默认实参程序、拥有者事务、native 物理参数投影和虚方法槽位。
  默认表达式看到前序实参和捕获的 self；多个绑定签名可共享声明，但各次调用
  的错误帧及临时存储独立。虚分派采用整个槽位的错误合同。
- 绑定函数体直接采用 bool/typed-out 的闭包入口 ABI，省去额外调用 thunk。
  参数与结果不经 Value 装箱，`@owned string` 保留生产者原指针。

主要代码为 `src/sem/xl6_sem.c`、`src/ir/xl6_ir_bound_method.c`、
`src/ir/xl6_ir_lower.c`、IR verifier、对应 C emitter，以及
`src/runtime/xl6_array_extract_value.c`。

## 测试合同

`tools/check_extern_owned_string.py` 新增四组正向用例和一个容器不变性反例：

- `extern_owned_nullable_channel_arguments.xl`：async Channel 的 Error/string
  nullable 实参，以及非空与 null 注入。
- `extern_owned_error_array_extract.xl`：非空/可空 Error 提取、越界、逃逸、
  丢弃返回和 96 个同步 OOM 前缀。
- `extern_owned_typed_bound_methods.xl`：可空接收者、重载、实例泛型、多个
  签名共享默认声明、隐式字段、逃逸接收者、默认闭包、void、tuple、继承、
  虚方法和可失败 override。
- `extern_owned_typed_bound_native.xl` 及 host：缩短/完整 C 标量签名、借用
  string 入参、接收者销毁、原指针、首次调用只含一次生产者分配，以及
  160 个同步 OOM 前缀。C 生成检查要求直接接管及融合的闭包入口。
- `bad_extern_owned_nullable_invariant.xl`：`array<string>` 不能因直接实参
  的 nullable 注入规则而变成 `array<string?>`。

独立 C 门扩展 Error 提取的身份、未绑定投影、错误保留、失败不移除与克隆后
提取检查，并覆盖 64 个 OOM 前缀。IR 破坏控制检查 bound target、接收者类型
和 closure role，必须在输出 C/header 之前拒绝。

## 性能边界

类型化 native 绑定调用的证据检查原指针、一次 malloc、零 calloc/realloc/
memdup/free；计数起点在绑定完成之后，包含该闭包的首次调用。
绑定本身的 closure 分配、receiver 保留、代码 lease，以及调用时的读取保护、
错误作用域和长度扫描仍存在。这里证明消除了装箱及字符串复制，不把这些
功能/分配检查解释为完整调用与手写 C 的计时等价。

动态 var/xcall、普通 string 赋值/捕获、任务发布与容器值语义仍有各自成本。
Error Array 仍存储原有私有 Error 盒，提取不复制 Error 文本；本轮没有把
整个容器改成原始 `xerror*` 槽位。

## 双平台验收

合并时同步主线 `e0c507a4f` 的构造器确定赋值检查；最终源码版本为
`d4c178b7b`。使用 Windows `driver-85` 和 Linux/WSL `linux-85` 的完整源码候选。Windows
构建含 1296 个编译单元，Linux 含 1298 个；收尾重新核验 Windows 的 2549 项
构建输入、两端对象及产物哈希。两端公开编译门的 68 项测试输入逐字节一致。

| 检查阶段 | Windows | Linux |
| --- | ---: | ---: |
| 公开编译链路：JIT、AOT、C 生成、拒绝项 | 166 / 166 | 166 / 166 |
| 独立 C 消费者及 C/IR 合同 | 29 / 29 | 29 / 29 |
| 任务相关回归 | 18 / 18 | 18 / 18 |
| 原审计的 27 个用例：emit/JIT/exe/AOT | 108 / 108 | 108 / 108 |
| 主线构造器合并回归 | 8 / 8 | 8 / 8 |
| 合计检查阶段 | 329 / 329 | 329 / 329 |

166 项中包含独立 C 门的汇总调用；329 是执行阶段数，不是独立语言用例数。
独立 C 门运行 O0/O2，继续拒绝 45 种非法 IR，且保留原有 OOM 与失败回滚检查。
构造器合并回归另含 42 项控制流、24 项语义/Typed 检查和 38 项 IR 破坏控制。
原审计目录保持原样；确认矩阵另存副本，仅将原生绑定方法的预期从“返回复制”
改为“保留原指针”，其余用例均按原断言执行。

原始回执与输入校验汇总：`build/owned-string/typed-boundaries-85/acceptance.json`。
其中记录 `acceptance-windows-85b`、`acceptance-linux-85`、各自的 `independent-C`、
`task-regression-{windows,linux}-85-focused`、
`confirmation-boundaries-85/{windows,linux}`、两端构造器合并回归，以及两份源码
构建记录的路径和哈希。

合并后的首个增量候选因验证脚本遗漏带 BOM 源码的首行 include 而混入旧对象，
已弃用。最终候选额外用 GCC `-MM` 核算 45 个编译器翻译单元，25 个受影响单元
全部重编，没有遗留旧对象。Windows 首轮还出现一次任务退役阶段失败；单独
复跑、新旧候选各三次对照及完整门禁重跑均通过。上述失败日志保留，不计入通过项。

C emitter 文档合同门通过。全仓 source-wiring 仍有既有目录登记缺项；C fixture
登记脚本还会把旧 `c_bridge_environment_host.c:50` 的反例路径字符串当作 include，
在 Windows 上报无效路径。本轮新增 IR 分片已通过 include 接入，新增 C fixture
由专项门显式引用；本验收不代表全仓登记或发布总门通过。

主线合入保留其他线程未提交改动；上述测试针对本次已提交的源码。
本轮未替换主工作区的 release 二进制，使用新功能须正常重建。
