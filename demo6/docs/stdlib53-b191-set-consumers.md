# Set 原生消费者与按需遍历

真实原生 Set 已接入通用浅复制、成员检查、动态操作和语言 foreach 的
驻留消费者。基础值按内容作为键，可变对象与容器按实例身份，Future 按
控制块身份。Set 语言工厂、导入与反序列化恢复仍待迁移，DEV-XRT-53 继续。

## 遍历和组件裁剪

静态 Set 遍历认证真实 owner 后创建独立快照，共享的迭代器分别拥有快照
和当前投影。遍历没有集合索引键，ordinal 由公共载体独立维护；EOF 与
checked 失败分开，失败保持可见。源集合清空或释放不改变已有快照。

通用遍历入口从 List 组件分出，分别选择驻留 List 和 Set 提供方，随后
才尝试真实 Array 和 wire 家族。不根据 HANDLE 标签解释外来 payload，
已认证 owner 的 BUSY 或状态失败不能落回 wire。只遍历 Array 或 List 的
程序不因通用入口与静态入口同文件而额外链接 Set。

## 复制和序列化输出

浅复制创建独立 Set 存储，保留完整逻辑域、投影和引用键。深复制的 Set
提供方加入 XRT 的同一个身份 memo，不另开递归复制；Array、List、Set
与 wire 对象混合图中的重复边保持共享关系。等内容但不同实例的对象仍
是不同 Set 键。图访问先匹配真实驻留 ops，再固定 owner，不能重读或
保留被 XRT 图操作保护的源 shell。

序列化投影将原生 Set 输出为稳定插入顺序的紧凑 Array，不保存哈希表
布局或进程内身份哈希。List 采用既定紧凑序列规则。Set 图复制和混合图
投影分别是可选组件；封闭 Array 或 List 图不自动链接它们。这是输出
投影能力，不意味着 JSON 或 XLON 的 Set 恢复已切换。

动态 Set 的 add、remove、take、clear、reserve 和 contains 使用实际
typed-value 操作。两个传入参数在任何退出路径均被消费，包括错误作用域
和清理作用域都无法进入的情形；传播 checked 失败，不直接 abort。

## 验证范围

`check_stdlib_set_consumers.py` 使用真实供应方存储，检查精确 NUL 与
UTF-8 长度、实例键、独立浅复制、混合图共享、快照与 sticky 失败、BUSY
拒绝以及保留调用者首错。O0/O2 分别执行八条分配和八条无诊断作用域
进入失败的完整前缀，并拒绝四个成功编译的错误实现。Linux sanitizer
只覆盖选定原生闭包，不涵盖整个编译器或发布库。

`check_stdlib_set_iteration_source.py` 从真实 XLIR 提取身份和函数 ABI，
把原生 Set 传入实际生成的静态与 erased foreach。严格 C 的 O0/O2 按
真实计划独立编译原生模块和驻留源；宿主输入工厂另按实际符号 owner
选择并记录闭包，不算作生成代码的依赖。检查 finally 与完整分配、提供方
无诊断拒绝前缀；Array 夹具验证排除 Set，普通 wire Set 另做真实 JIT。

证据保存在 `E:/xlang-validation/stdlib53-b191-*` 与 Linux 对应目录。
本批不启用语言 Set 工厂，不完成 Set 方法、导入、恢复或 Dict 的迁移，
不关闭 53 模块总验收。CRT 0 项、XRT 19 个未决策组及对象池最后的安排
保持不变。
