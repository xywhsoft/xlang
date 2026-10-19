# XLIR 零负载参数合同

`future<null>` 与 `Promise<null>` 使用现有逻辑空值域；不新增公开基础类型，
也不把语言的 `null` 改成 C 指针或可寻址标量。

- 逻辑空值不拥有资源，规范所有权为 trivial；其物理值 C ABI 仍不支持实体存储。
- 原生泛型描述符的有数据输入采用 `generic-value-address-borrow`；精确的空值输入
  采用独立的 `generic-zero-payload`，对应零长度 Future 值描述符与 `NULL` 参数。
- 空值 SSA 生产者必须来自精确类型的 `null` 字面量。其结果只可交给匹配的同步
  原生调用，不可存储、返回、捕获、合并或跨挂起保存。
- XLIR 在 C 与头文件输出前拒绝错误物理投影、类型、生产者及消费者；Cgen 不按
  `Promise` 名称推断规则，不创建占位 C 变量，也不对空值取地址。
- `await future<null>` 沿用现有无数据完成与错误结果合同，取消与清理顺序不变。

常规验证入口为 `tools/check_stdlib_five_groups.py --stage promise`，覆盖直接完成、
重复完成、取消、关闭、固定工作队列中的异步完成及 IR 篡改后的零字节输出拒绝。

这不宣称泛型类字段默认表达式的单态化问题已经解决，也不新增任意 null 形参、
null 存储变量或 null 返回值的物理调用 ABI。
