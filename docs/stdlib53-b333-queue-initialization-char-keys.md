# 队列初始化与字符键排序

不可空并发队列必须在声明时初始化，延迟创建使用 nullable 队列。
OrderedMap 的字符键使用既有原生 char 描述符与 XRT typed tree 比较，
不转换为 uint64，也不在 Cgen 生成比较器或容器算法。

## 队列声明合同

SpscQueue、MpscQueue、MpmcQueue、WaitQueue 及 Queue 别名均没有默认容量。
因此局部变量、全局变量和类字段的不可空声明都必须包含初始化表达式。
类型别名遵循相同规则；声明后再赋值不能代替声明初始化。
构造函数仍须显式提供正整数容量，运行时非正容量报 RangeError。

```xlang
SpscQueue<int> ready = SpscQueue::<int>(2)
SpscQueue<int>? deferred
deferred = SpscQueue::<int>(2)
```

nullable 队列保留 null 默认值。调用成员前需要正常的非空流收窄；字段读取
先保存到局部快照，避免把可再次读取的对象字段当成稳定的局部绑定。
没有引入空原生句柄、隐式容量或新的参数修饰。

## 字符键的原生表示

Typed IR 与运行时共用 `xl6_tree_key_abi.h` 的闭合排序标记，新增独立的 char
标记。Typed IR 的构造配方、lowering 和验证器均保存精确的字符键排序合同。
Cgen 继续消费规范化布局并调用既有 Tree provider。

运行时使用 `xl6CharTypeAbiV1` 检查字符物理布局，再由原有 char 描述符比较
uint32 Unicode 标量。该检查不引用可选描述符工厂，纯整数 OrderedMap 不应
因此保留字符实现。NUL、ASCII、汉字和补充平面字符使用相同的标量顺序。

## 验收范围

本轮产品验证覆盖显式初始化、nullable 延迟创建、字符键操作和独立程序运行。
裁剪分别检查双端真实 Tree provider 对象不依赖字符工厂，以及 Linux 真实编译器
产物计划只为字符键选择字符描述符。TCC 的最终独立程序没有完整符号表，不能
把空的符号查询结果当成裁剪证明。

Windows 与 Linux 的实际产品总门各完成 319 项清单：310 项执行通过、
1 项预期拒绝、7 项需专用宿主、1 项需另一平台；专用宿主由相应门禁另行验证，
平台条件不计作通过。独立 AOT 清单各有 309 项执行通过、1 项预期拒绝、
8 项需宿主、1 项平台条件。可执行程序均经过迁移目录后的独立运行检查。

新增专项在 Windows 完成 78 阶段，Linux 完成 86 阶段；每端 7 个程序经过
JIT、模块和独立 AOT，保存的生成 C／XRT 实现均以严格 O0／O2 编译。
10 个语义负例包括 15 处无初始化声明。Linux 另有 100 项字符键 IR 破坏对照，
加上既有 SlotMap 269 项和 Array 16 项，均拒绝且 C／头文件输出保持为空。
142 项原生成员签名与 228 项静态签名绑定核对保留；绑定不等同运行分支覆盖。

实际构建使用 `stdlib53-b333r1-source` 冻结输入。最终产品报告为
`D:/xlang-validation/stdlib53-b333r3-products-win/results.json` 与
`/var/tmp/xlang-stdlib53-b333r3-products-linux/results.json`；独立 AOT 报告为
同平台的 `stdlib53-b333r1-aot-*/results.json`。两个修订过的断言夹具使用
显式 `--fixture-source`，没有替换冻结产品源码。早期错误断言、无效符号查询和
缺少测试编译 include 路径的失败报告原样保留；最终总门重跑完整清单，并对
复用的专项逐项重算输入、产物及日志哈希。旧产品对照复现字符键 IR 拒绝和
五种队列无初始化声明被错误接受。

完整 53 模块接入仍进行中；其他键域、调度器全局 OOM 和所有运行分支不能由
这组回归替代。对象池继续放在最后。

OrderedMap 的 `c_int` 与 bytes 键已再次复现语义准入后被 Typed IR 拒绝，
属于后续原生键域审计工作。本轮只补齐已有 char 描述符的排序合同，不把其他
可比较类型一概宣称为已经支持，也不为通过清单而缩小语义准入范围。
