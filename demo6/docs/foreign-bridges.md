# C 扩展的资源和数据桥接

`cstring/cbytes` 继续处理文本与二进制结果。下面两个独立入口解决 C
资源生命周期和容器表示；不会改变普通字符串读取或所有 `extern var` 的语义。

## 原生资源

`xl6ForeignResourceCreateV1(raw, releaser)` 返回私有 `var` 资源令牌。
`releaser` 是 `xcall` 产生的 callable，接受一个 `point`，无返回值；令牌持有
该 callable，连同其捕获和代码租约一起参与退役图。最后一次释放或模块退役
会在清空图之前调用释放器一次，然后释放 callable 和上下文。释放器失败也不会重复调用。

成功才接管 `raw`；失败时生产者必须释放自己的原始资源。语言绑定可用
`try/finally` 和成功标志保护分配、callable 装箱及接管的完整事务，参见
`test/cases/xlir_foreign_bridges.xl`。也可在 C 生产者中实现同一回滚合同，
但须确保其运行时依赖进入包的构建闭包。不能省略接管失败时的原生清理。

`xl6ForeignResourcePointerV1(token)` 返回一次同步操作所用的借用指针。
调用期间必须保留令牌；不能将借用指针保存到异步操作中。
语言 class 可以在私有 `var` 字段保存令牌，其余方法通过该入口取得指针。
SDK 声明位于 `xl6_foreign_resource.h`。

这是明确的叶子载荷合同：原始 C 载荷不能隐藏语言对象、回调或异步图的
拥有边。释放器自身的引用已由令牌追踪。网络客户端等复合资源仍须提供
完整的 XRT 所有权、关闭和准备协议；不能用本入口或把全部 native point
自动标为叶子来绕过退役检查。未建模的旧 native-backed class 仍会被拒绝。

## 数据快照

`xl6ForeignDataSnapshotV1(value)` 将语言数组及 List/Set/Dict 的数据图转换为
普通 XRT 数据表示，供原生编码器或配置 API 使用。Array/List/Set 对应数组，
Dict 对应对象；嵌套图使用同一份 XRT 遍历上下文。SDK 声明位于
`xl6_value_copy.h`。

输入是借用值，结果是独立拥有的数据快照，须由调用方释放。C 端修改快照
不会回写语言源容器。该操作需要遍历和分配，应在需要普通 XRT 数据的
桥接边界调用；资源和 callable 使用各自的通道，不应作为数据快照传输。
转换失败遵循既有图转换的错误和清理规则，不发布部分结果。

## 验证

Windows 执行 `tools/check_foreign_bridges.ps1`，覆盖 C 接管失败、逐分配点
故障、别名释放、语言局部及全局 class、嵌套容器快照，以及 JIT/AOT。
测试目录独占且保留日志。`xcall` 是正式的显式回调装箱方式，无需开放隐式
`function -> var`。
