# XLIR 原生 Future 映射合同

当前实现批次：v207，唯一一轮 41 项统一复测全部通过。本文件描述生产合同，
不将这一批通过等同于完整原生服务/混合拥有图或长期任务完成。

## 已确认的问题

v206 的真实生成代码在受控 Promise 上可以完成 TCP 标量映射和地址列表
映射，但待完成期间未取得模块代码保活引用；地址列表结果持有的生成析构、
身份回调也没有独立代码能力。功能成功和最终内存平衡不能证明这些指针可在
模块 Close 后调用。失败探针保存在 `build/tmp/xlir_launch_v207_network_code_discovery2`。

## 单一语义合同

Lowered XLIR 的 `nativeFutureMapAbi` 从可达 CallTarget 的精确 runtimeBridge
身份导出，覆盖 TCP dial/stream/server、TLS dial/stream/server、异步解析和
UDP 接收。它与返回 TypeId、原生 owning class layout 和 classStorageAbi
共同构成发射输入，不从 C 符号拼写或链接失败推断。Typed IR 保持 NONE。
验证器、C/header 发射和产物计划必须拒绝缺失、过时或未知的合同。

该合同要求：

1. 映射层拥有独占源生产链。取消输出时请求取消源，但不删除已受理的源回调，
   不跳过清理。成功才执行 mapper，失败、取消、关闭透传相应终态。
2. mapper 所在的常驻回调 frame 取得或继承同一模块的代码能力。源结果在
   回调返回之前由独立真实源引用保护，不依赖某个捕获槽尚未被清理。
3. 标量、字符串、xvalue 结果盒使用常驻 payload policy。结果中的原生 class
   使用已有 ClassRuntimeV2 的创建、身份绑定和提交，使生成析构/身份回调的
   代码能力持续到所有字段释放之后。Future 与其中独立保留的对象寿命不同。
4. 参数为端口整数或空值的 mapper 使用闭合的 trivial capture layout；TLS
   拥有型上下文不是 trivial。它沿 V1 回调的明确 native owning context 路径
   保存与释放，并在仍拥有未知 native 图时拒绝 Trace 和集合准入。

## 常驻物理生命周期

`FutureCallbackRuntimeV3.StartExclusive` 是增量入口，V1/V2 的布局和非独占
Start 行为保持不变。它沿现有单一 frame 增加一个 certified CancelWatch：
注册实际持有一个 frame 引用，frame 实际持有注册，注册再持有取消令牌。
所有边均可见；注册、发布、通知、退休进行中拒绝检查。准备阶段不获得取消
或删除业务回调的权限。

取消可能同步完成源，并重入 frame 的正常退休路径。取消通知先取得独立源
引用，在 frame 锁与 mutation 之外请求取消；Unwatch 在锁外等待或完成自身
回调退休。frame 和代码 lease 的最后释放均在常驻返回路径，不依赖已卸载
模块中的析构尾调用。失败注册保留私有捕获供 Discard；拥有型 native 参数
只在受理成功后转移，失败仍由原调用方清理。

## TLS 共用组件

TLS transport 只负责原生 stream/lease 构造与释放。共享模板通过明确的
Future carrier ABI 选择 legacy adapter 或 resident-exclusive adapter。
XLIR 路径直接使用 xfutureresult / xpromise，不再构造 legacy result、保存
legacy payload destructor、再将其转回 XRT Future。旧后端保持独立显式入口，
不是 XLIR 的运行回退路径。

## 验证范围与未完成边界

专项覆盖 IR 变异拒绝、真实生成模块、输出取消与源完成竞争、分配失败回滚、
真实 DLL 在 Close 后完成映射及卸载、Future 释放后仍存活的独立 class 结果。
已有六个网络程序的完整 JIT/AOT/EXE 和同进程公开生命周期继续保留。

受控 Promise 的实际生成适配器卸载，不等于带活动 engine/resolver 的完整
模块卸载。模块服务 trace 仍必须对未知 native 拥有状态返回 opaque；TLS
上下文、原生服务与混合环的精确接入、全语言/FFI、发布与部署、平台验收及
legacy 退役仍是长期任务的未完成范围。不得通过返回空图或删除保活引用结项。

## 版本与证据边界

v207 的原始 baseline 沿用 v206 发布文件哈希，但实际首次源码构建已经记录了
不同的 release/xl.dll。后续核对还确认 release/xlc.dll 与 v206 不同；文件时间
为本地 2026-09-15，时间本身不证明变更来源。原 baseline 和首次失败工程检查
保持原样，另以 `build/tmp/xlir_launch_v207_environment/results.json` 记录当前
依赖和首次构建记录。旧两个 DLL 已从既有构建目录找到逐字节匹配副本，旧、新
二进制同时保留，不回写发布目录，也不将环境一致性视作同源发布验收。

身份绑定质量检查改为要求真正发出的 `xl6ClassRuntimeV2Get()->BindIdentity`
调用，不再依赖旧直接 API 字符串或头文件中的函数声明。两个既有类身份用例
另以当前源码驱动检查确定性发射和新入口；该轻量检查不冒充旧发布驱动的完整
运行差分套件。能力矩阵按实际源码重新生成，没有扩大延期 API 清单。

最终复测 41/41、0 失败、0 未运行，4219 输入前后指纹相同；工程检查 13/13。
最终证据为 `build/tmp/xlir_launch_v207_evidence/results.json`，其中保留原失败
发现、修复过程、全部旧门和新增探针，并明确列出仍未完成的长期目标。
