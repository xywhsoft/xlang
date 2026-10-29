# OrderedMap 与默认 Queue

更新日期：2026-10-12

已批准的有序映射公开名改为 `OrderedMap<K, V>`，旧 `AvlTree` 名称退出
语言登记。默认 `Queue<T>` 与 `MpmcQueue<T>` 是同一种类型，不是两个
可转换的包装类。本批复用既有原生存储、拥有策略和方法实现，不修改 XRT。

## 公开类型

```xlang
func sameQueue(Queue<string> input) as MpmcQueue<string>
    return input
end

auto queue = Queue::<string>(4)
MpmcQueue<string> explicit = sameQueue(queue)
Queue<string> same = explicit
assert(same.tryPush("你\0好") == queueStatus.ok)
auto status, item = queue.tryPop()
assert(status == queueStatus.ok and item != null and item == "你\0好")

auto ordered = OrderedMap::<string, int>()
ordered.set("z", 3)
ordered.set("a\0b", 2)
ordered.set("a", 1)
assert(ordered.keys()[1] == "a\0b")
```

两个队列名共享登记 ABI、规范类型名、元素 schema、运行时句柄及并发角色。
赋值、传参、返回不创建转接队列；close、reset 和读写作用于同一个对象。
`Queue<string>` 不能赋给 `Queue<int>` 或 `MpscQueue<string>`；以两个别名
分别声明相同函数重载仍被拒绝。SPSC／MPSC 的角色限制没有放宽。

`OrderedMap` 仍使用 AVL 实现，公开名不绑定具体平衡树算法。内嵌 NUL
按精确长度参与字符串键比较。keys／values 为现有有序快照；clone 创建
独立树存储，槽位按各自值／引用合同复制，不意味着对象深克隆。

## 统一 IR 与生成边界

类型模型统一规范名称；Sem 在结构化类型比较中复用这一规则，仍递归
比较泛型参数。Queue 的登记 ABI 显式复用 MpmcQueue 的身份，XLIR 对同一
实例化只建立一个具体类型。构造器名自身与实例化类型分开登记，不能把
二者误判成重复具体类型。

OrderedMap 的公开语言名称更新，不改物理 `avl-tree` ABI、原生方法枚举
或 resident 提供方。Cgen 没有新增函数名分支、队列转换、树算法或兼容路径。
活动源码夹具和公开示例同步迁移，历史验收记录保留原名称。
工程守卫和能力矩阵生成器使用当前公开名；名称专项复核守卫要求的
诊断与真实 verifier 一致，不把这项检查计成全仓守卫通过。

## 验收

Windows／Linux 使用本批源码编译器，分别完整构建 302／301 个翻译单元。
编译器源码、包含文件、验收夹具和产品输入在提交前与选择索引逐字节复核。

| 专项 | 每端结果 |
| --- | --- |
| 38 份源码夹具 | 37 份成功生成 C；1 份 thread 非复制测试在原位置被正确拒绝 |
| 11 份 JIT 夹具与公开示例 | 通过，覆盖类型别名、树投影、引用元素、nullable、Set／Dict 配方 |
| 5 项错误合同 | 旧树名、错元素、错角色、错泛型数量和重复别名重载全部拒绝 |
| 真实树 IR 合同 | 4 个工厂、39 个操作，210 项损坏合同在生成前拒绝 |
| 严格 C O0／O2 | 混合与 Queue-only 四份独立程序，各 1000 次模块生命周期，内存平衡通过 |
| 按需裁剪 | 使用真实 artifact plan；Queue-only 不含 SPSC、MPSC 和树提供方符号 |
| 公共 API 模块与 AOT | 两份实际程序完成 relocation、生成并执行 |
| SlotMap 快照回归 | 780 阶段、610 次完整失败前缀和 499 个损坏合同通过 |
| Array／List／Set／Dict 原生值回归 | 631 阶段、598 次完整失败前缀、1344 个损坏合同及 5 个已编译坏实现拒绝通过 |

名称专项每端 169 阶段。证据位于 Windows
`E:/xlang-validation/stdlib53-b216-*` 与 Linux
`/var/tmp/xlang-stdlib53-b216-*`。验收入口：
`check_stdlib_container_names.py --root <frozen-demo6> --public-directory
<fresh-source-build> --out <fresh-output>`。

AOT 使用现有 VFS 清单展开的物理私有源码 SDK，不是重建发布 VFS。
本批没有新增并发压力、全线程 OOM、性能基准或发布 DLL／SO 验收。
文档门禁的 14 条历史诊断保留，本批不增加诊断。

## 后续边界

OrderedMap 的 `getInto`、`takeInto` 与边界输出方法仍待改为精确多返回值。
WaitQueue／Channel 的现代结果接口和生命周期、队列并发压力、SlotMap
成对迭代及原生值 nullable／cell／frame／持久化、发布 VFS 和三组总门
继续进行。公开命名完成不等于整个容器类别完成。

`PublicContainerNames=true`、`QueueAliasIdentity=true`、
`PrunedSourceExecutables=true`、`PublicAotExecutable=true`；
`ShippingLibraryAcceptance=false`、`Full53Acceptance=false`。
Set 的可变对象／容器按引用身份、基础值按内容规则不变；CRT 未决策 0、
XRT 未决策 19 组和对象池最后不变，见
[标准库决策清单](标准库决策清单.md)。
