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
| 可空 @no_raise 直接入口 | 不增加 foreign error scope；生成调用及指针/长度构造 |
| 可失败入口 | 既有 foreign scope、NULL/长度检查，必须单列成本 |
| xcall、动态方法、保存为函数值的实例方法 | 使用既有动态表示/装箱/解箱，可分配及复制 |
| 普通赋值、捕获、容器、任务结果 | 普通 string 既有值语义，不承诺全部零拷贝 |
| 普通字符串槽位搬移 | 继续使用既有 Slot runtime 发布/清空协议，不能把普通生命周期调用算作已消除 |

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

可运行示例：release/examples/extern_owned_string.xl 及其 native C 文件。
测试入口 tools/check_extern_owned_string.py；使用 --driver 指定 Windows
公共源码驱动，或 --cli 指定正常 xl CLI，并提供全新的 --directory。

门禁覆盖原指针、分配/释放计数、空字符串与 NULL、无诊断失败、失败非终止
载荷、OOM 前缀、函数值、动态/协议方法、包导入、worker、JIT/AOT 与生成原型。
普通借用返回继续复制，错误类型/构造器/多返回/冲突声明必须拒绝。
