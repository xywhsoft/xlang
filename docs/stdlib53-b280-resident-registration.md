# 驻留导出归属与源码构建校验

五个既有 Stack／Sequence 接口补齐精确的驻留组件归属。它们支持
动态 Stack 的角色检查、转换策略和返回 Array 身份，不增加 xlang
公开标准库 API，不修改容器语义或生成 C 的转换规则。

## 接口归属

- `xl6SequenceBeginErasedRoleV1`、`xl6SequenceBindProjectionRoleV1`、
  `xl6SequenceBindStackResultArrayV1`、`xl6SequenceStackResultArrayV1`
  归属 `SEQUENCE`。
- `xl6StackEraseV1` 归属 `STACK_ERASED`。它仍被原生测试使用，
  与现有 V2、拥有值 Take 入口共享实现，不新增兼容桥接层。

## 验收条件

目录总门逐个编译实际驻留提供方，核对唯一导出定义、精确登记、
组件归属、全部跨组件依赖和闭包。新增的五项分别经过漏登记与
错误归属反例；原有重复定义、缺失组件、失效依赖和宿主 checked
清理接口反例继续保留。

使用编译器源码构建时，目录门先核对全部编译输入、头文件和对象
哈希，再逐项比较实际构建源码与消费快照；旧编译器对象不能搭配
新登记表冒充当前验证。Stack 的源码 C／JIT／模块／AOT 与工厂
裁剪专项验证该登记不改变既有执行和按需引入行为。

这项验收不关闭完整容器转换、其他动态／Native 编解码组合、
工作线程与对象图故障、模块组合、真实卸载及 CLI／DLL／VFS
发布总门。WaitQueue 保持 MPMC 与 `(status, T?)`，对象池最后；
CRT 无未决策、XRT 19 组不变。
