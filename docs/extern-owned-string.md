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
| 具体类实例方法绑定到 ABI 精确匹配的固定 typed 函数值 | 编译期选择重载并捕获接收者；类型化调用保持 owned 返回的原指针，不装箱参数或结果 |
| xcall、擦除到 var 的方法及其他动态调用 | 使用既有动态表示/装箱/解箱，可分配及复制 |
| 普通赋值、捕获、容器、任务结果 | 普通 string 既有值语义，不承诺全部零拷贝 |
| 普通字符串槽位搬移 | 继续使用既有 Slot runtime 发布/清空协议，不能把普通生命周期调用算作已消除 |

固定签名函数值可连同原生 C 标量参数直接交给 `task.spawn`，包括返回
`@owned string/string?` 的 extern 和类静态入口。全部 20 种 `c_*` 标量
使用原 C 类型作为任务帧字段和调用参数，保留宽度、符号及平台差异，不经过
i64/f64 归一化或动态装箱。字符串接管、Future 发布/读取和错误清理沿用
各自的现有合同，不能据此声称整个任务路径零复制。`byref/out` 调用者存储
和原生 C `c_vararg` 函数值仍不能直接提交给 `task.spawn`。

顶层和类静态 extern C 的 `c_vararg` 可以保存为函数值，也可以用 `func(c_int, c_vararg) as string`
这样的完整类型传参。每个调用点直接展开 C 实参，执行一次原生间接调用，
不使用 va_list 转发 thunk、装箱数组或额外的字符串 owner。整数/float 由 C
执行默认提升。字符串尾参借用原缓冲，通过 NUL 检查后传指针；检查失败时
不进入 C，即使 C 入口标记了 `@no_raise`。

固定签名的 extern 默认参数也可以收窄为较短的函数值签名，例如：

```xlang
extern "native_text" @owned func Text(c_int mode=7) as string
func() as string producer=Text
assert await task.spawn(producer)=="native text"
```

默认表达式在每次调用时计算；失败时跳过 C 生产者并执行普通清理。泛型函数
中的 task.spawn/all/race/whenAny 在具体化后重新验证参数、结果和拥有权合同，
不增加运行时泛型分派。可空函数值在已有非空流证明下可直接交给 task.spawn，
包括 typedef 别名；未检查或赋值后证明失效的可空函数仍拒绝。

task.all 支持原生 C 标量 Future 的字面量列表和 array<future<T>>。它沿用已有
结果数组的装箱语义，使用原生标量 profile 和类型身份，保留无符号全范围与
平台相关宽度；c_bool 沿用动态存储中的共享 bool 单例规则，类型化读取返回
c_bool。聚合的结果数组、元素装箱和 Future 读取仍有原有成本，不能称为整个
聚合过程零复制或与手写 C 等速。

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

具体类的实例方法可直接绑定到参数和返回 ABI 精确匹配的固定、`...` 或 `***` 函数签名，包含非空流证明后的可空接收者、
按签名选择的重载，以及由参数/结果类型闭合的实例泛型方法。省略的尾部形参
必须有默认值，默认表达式仍在每次调用时执行，并使用捕获的接收者和前序实参。
虚方法保留虚分派和整个槽位的可失败合同。示例：

```xlang
class Native
    private native point handle
    extern "native_open" create()
    extern "native_close" destroy()
    extern "native_text" @owned func Text(c_int mode=c_int(7)) as string
end
Native value=Native()
func() as string read=value.Text
string text=read()
```

数组/字典可变参数绑定保持参数包在最后一个物理参数位置。若精确签名省略
包之前带默认值的固定参数，默认值在同一个调用体内计算，已有参数包直接
借用，不重新装包。返回签名可保留 `cstring/cbytes`；若明确要求普通
`string/bytes` 返回 ABI，则在同一个调用体内复制相应结果叶子。

方法组也可直接转换为可空的函数签名，例如
`value.Text as (func() as cstring)?`。编译期先完成精确签名绑定，再提升为
可空指针，只求值一次接收者。泛型类方法接收可空函数参数时，同样支持直接
传入方法组、非空函数值或 null；重载、访问、约束与具体实参 ABI 继续校验。
本轮修复及安全验证见
[可空和可变参数方法绑定](extern-owned-callable-final-fixes-2026-10-29.md)。

精确函数类型还会传入条件表达式和 `??` 的方法分支。例如
`func() as cstring read=pick ? first.Text : second.Text`，以及
`func() as cstring selected=optionalRead ?? first.Text`，都在编译期完成分支内
绑定；只求值实际选中的接收者。变量、赋值、返回值、函数实参和整个表达式的
显式转换使用同一规则，支持嵌套表达式和已有非空流证明。

回调默认值也可直接写成 `func Read(func() as cstring callback=self.Text)`。
默认值在原声明作用域检查访问和约束，每次需要默认值时绑定一次；回调可随
返回值逃逸。别名、可空回调、继承、泛型、前序参数及 `...`/`***` 沿用同一
精确绑定，不必手写 `as`。默认值仅经缩短签名的函数值使用时同样有效。
完整程序见 [条件回调](../test/cases/extern_owned_bound_compound.xl)、
[回调默认值](../test/cases/extern_owned_bound_callback_defaults.xl) 和
[嵌套及泛型上下文](../test/cases/extern_owned_bound_callable_contexts.xl)。

这些上下文保持精确 `cstring/cbytes` 和 `@owned string` 返回 ABI；显式要求
普通 `string/bytes` 回调结果时，继续只复制所需的结果。任意 `var` 分支不能
凭另一个合法方法分支取得 compiler-only owner ABI 的静态授权。

绑定只求值一次接收者，闭包保留它直到最后一个引用释放。绑定本身仍有普通
闭包分配、引用与代码 lease 成本；调用保留闭包读取协议、错误作用域及必要的
strlen。本路径消除动态参数/结果装箱和返回字符串复制，没有宣称整个闭包
调用与手写 C 等速。生成的绑定函数直接使用 typed 闭包入口 ABI，省去额外
的转发 thunk，重载和泛型判断均不进入运行时。

既有 xrtValueStringTake 会把缓冲 realloc 到精确长度，跨尺寸类时可能移动并
复制；方法一旦擦除到动态 Value，仍可能经过字符串投影复制。这些成本不属于
@owned 接管步骤，不能用类型化路径的结果声称所有动态路径都零损耗。

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
