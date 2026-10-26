# C 自有字符串直接返回普通 string

`@owned` 是 extern 返回值合同。它使 C 的自有 `char*` 直接进入普通
`string`，不需要 C 包装函数、`cstring.take` 或额外的 C owner 分配。

```xl
extern "foo" @owned func foo() as string
string text=foo()
println(text)
```

## 内存与 ABI

生产者返回的非空指针必须是当前 XRT 堆的分配地址，可以由同一个运行时
`xrtFree` 释放；成功返回时内容是有效 UTF-8、NUL 结尾，且字节长度不超过
`INT64_MAX`。交出后生产者不能继续修改或释放。编译器不扫描 C 实现来猜测
所有权，也不在释放时探测分配器。

普通字符串仍为 Data/Size，释放仍用 xrtFree。C 原型保持 `char* foo(...)`，
不会误声明为返回结构体。接收直接保存指针，并执行一次 strlen 取得长度。
指针不带长度时无法消除这次扫描；本功能不额外做 UTF-8 全文验证。

原始 malloc 即使与 XRT 使用相同 CRT，也不满足这个合同：XRT 维护私有块头
和池。提供源码的库应利用已有分配钩子完整接入 XRT；其他释放器继续使用
cstring。字符串布局没有释放器指针、来源标签或逐次读取检查。

## 空值、错误与释放

`as string` 遇到 NULL 是接收失败；保留 C 原始错误，没有诊断时仍形成失败。
`as string?` 的 NULL 是正常空缺。非空零长度分配仍属于字符串并须释放。

```xl
extern "optional_text" @owned @no_raise func Optional() as string?
```

`@no_raise` 只允许与可空的 owned 返回组合，避免非空接收可能失败却被声明
为不抛异常。它是完整入口的不失败承诺，不是跳过错误处理的优化开关。

非空返回无论调用成功与否都转移清理义务。失败先释放原指针，再传播原错误，
不先 strlen 失败载荷。接收成功后由既有普通字符串生命周期处理赋值、提前
返回、异常、循环、捕获和任务帧。没有 C owner 的第二次分配。

属性只用于单一 string/string? 的 extern 函数、native 方法或 extern getter。
不用于构造器、其他返回类型、输出参数或任意多结果 C ABI。未标注的现有
extern string 返回继续复制借用内容。重复 C 符号不能声明冲突的拥有权。
借用返回即使标记 `@no_raise`，接收复制仍可能分配失败，完整语言调用保留
可失败结果；只在 C 调用成功后读取载荷，复制失败不会形成无效的普通 string。
注册过的 resident 桥接还须由其封闭合同明确允许自有 char*，属性不能覆盖它。

包公开声明使用 `export extern "foo" @owned func Foo() as string`，导入方直接
调用原 C 符号。语言函数的 `export "public_text" func Text() as string` 则
保留普通语言字符串的 Data/Size 导出 ABI；C 调用方应包含实际生成的头文件，
消费完成后用同一 XRT 的 xrtFree 释放 Data。二者不能互相猜测为 char* ABI。

## 性能边界

| 路径 | 行为与成本 |
| --- | --- |
| 直接 extern、静态方法、直接 native 方法/getter | 保存原指针，无载荷复制或 owner 分配 |
| 普通 extern 函数保存为 typed 函数值 | 接管后按普通 string 返回，无接管复制 |
| native 方法经 protocol 分派 | 同一接管合同，保留原指针；既有分派成本仍在 |
| 可空 @no_raise 直接入口 | 没有可失败的 C 文本实参准备时不增加 foreign scope；生成调用及指针/长度构造 |
| 可失败入口 | 既有 foreign scope、NULL/长度检查，必须单列成本 |
| xcall、动态方法、保存为函数值的实例方法 | 使用既有动态表示/装箱/解箱，可分配及复制 |
| 普通赋值、捕获、容器、任务结果 | 普通 string 既有值语义，不承诺全部零拷贝 |
| 普通字符串槽位搬移 | 继续使用既有 Slot runtime 发布/清空协议，不能把普通生命周期调用算作已消除 |

顶层和类静态 extern C 的 `c_vararg` 可以保存为函数值，也可以用 `func(c_int, c_vararg) as string`
这样的完整类型传参。每个调用点直接展开 C 实参，执行一次原生间接调用，
不使用 va_list 转发 thunk、装箱数组或额外的字符串 owner。整数/float 由 C
执行默认提升。字符串尾参借用原缓冲，通过 NUL 检查后传指针；检查失败时
不进入 C，即使 C 入口标记了 `@no_raise`。

`auto producer = Class.Func` 与顶层具名函数值使用相同绑定路径，支持闭合的
完整签名、显式签名选择重载、继承和包导入，不增加实例接收者或动态方法层。
固定参数签名沿用既有适配，C 可变参数保留原生入口。泛型函数值继续遵循
普通具名函数值的签名闭合限制；private 和实例方法不能借此绕过访问或接收者检查。

这类函数值复用已有 closure 分配和代码 lease。拥有权和效果写在其静态
调用元数据中；重绑定自有/借用生产者时随值变化。调用时读取效果，字符串
返回还需按拥有权分支接管或复制。可失败调用及文本实参准备保留 foreign
scope；普通 string 的布局和读路径没有新增判断。本路径不宣称整个调用
与手写 C 等速。动态 `var`/`xcall` 仍需先收窄为普通固定调用签名；扩展资源
结果、特殊 native 参数和实例接收者不通过原生可变参数函数值隐式擦除 ABI。

尤其是既有 xrtValueStringTake 会把缓冲 realloc 到精确长度，跨尺寸类时会移动
并复制；绑定实例方法还会经过动态结果的字符串解箱复制。这些不是 @owned 的
接管步骤，本轮未调整其设计，也不能用本轮结果声称这些路径零损耗。

## XACME 试用

Store.LoadAccount/LoadCert/LoadCertCa 直接声明上游 xrtAcmeStoreLoad*，没有
语言转发体或 native 转发体。Client.AccountPem 声明调用必要的 resident
资源检查入口，并直接接管其字符串，删除 cstring 包装。

复合资源的使用域、关闭和回调图继续执行；本功能不以字符串直通为由绕过这些
安全协议。双结果 Issue/LoadGrant 保留已有事务拥有者和 cstring 合同。

## 验证与范例

本批双端验收与性能边界见 [专项验收](extern-owned-string-acceptance.md)。

可运行示例：release/examples/extern_owned_string.xl 及其 native C 文件。
测试入口 tools/check_extern_owned_string.py；使用 --driver 指定 Windows
公共源码驱动，或 --cli 指定正常 xl CLI，并提供全新的 --directory。
源码驱动自动读取相邻 build.json；正常 CLI 的完整验收还须用
--runtime-build 指定对应源码运行时的构建回执。缺少回执时结果明确标记
independentCConsumer=false，不能视为独立 C 头文件验收。

tools/check_extern_owned_string_public.py 将生成的 C、原生生产者和 C 消费者
作为独立翻译单元编译。消费者只包含真正输出的头文件，不经过 C sidecar
注入；覆盖可空非空值、多返回值、普通 string、无诊断失败以及 120 个
OOM 前缀，并分别运行 O0/O2 产品。

门禁覆盖原指针、分配/释放计数、空字符串与 NULL、无诊断失败、失败非终止
载荷、OOM 前缀、函数值、动态/协议方法、包导入、worker、JIT/AOT 与生成原型。
普通借用返回继续复制，错误类型/构造器/多返回/冲突声明必须拒绝。
