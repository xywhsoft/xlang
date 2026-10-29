# B142：Thread 迁移到驻留运行时与语言专项验收

本批继续已批准的三组 53 模块任务，迁移既有 Thread 支撑，不新增 X15/X16
公开 API。B141 的原生生命周期保持不变；持锁对象仍通过宿主 checked
清理／恢复接口显式解锁、重试，不自动解锁或丢弃资源。

## 实现边界

- Cgen 不再生成 Thread 的 owner、入口、引用计数、析构、状态或借用函数体。
  六种已验证 RuntimeOp 机械调用 `xl6Thread*V1`，传递完整 TypeValueId。
  线程算法复用 XRT，闭包调用与代码驻留复用既有 Closure V2。
- XLIR 新增 `thread-reference-v1` 闭合 Sequence schema。私有驻留操作表、
  完整身份及 nullable 状态共同决定准入；相同 TypeId 不证明原生来源。
  策略只拥有不可变类型元数据，不持有源模块或工作线程／闭包边。
- Stack、Tree、Tree→Array 和三种 concurrent Queue 使用同一合同。嵌套
  schema 仍复用现有递归元数据。实际 Thread→Closure 的拥有图独立准入；
  这不是为任意闭包捕获或外部 owner 操作表颁发图证书。
- 实际组件目录新增 THREAD、ARRAY_THREAD_ADMISSION，并从固定原生符号
  和完整 schema 选择依赖。API 注册、图解析和两端 shipping 源清单已接入。
  不以共享库中恰好存在某个导出符号推断独立产物已经正确裁剪。

批量验证修正了三个语言层缺口：局部 Thread 的真实空句柄默认值；
typed Array `pop/take` 的完整类型推断与拥有返回检查共用合同；nullable
Thread 在非空分支采用已证明的流类型。Thread 仍不可由普通赋值或索引借用
复制，拥有型提取不会创建第二个 OS 线程。不恢复参数修饰或公开 byref。
缺失 callable 产生 ValueError；测试检查错误类别，不依赖旧的生成器文案。

## 同源构建与测试

生产输入来自 `stdlib53-b142-source-stage-dev6/frozen/demo6` 的独立 index
快照，未混入 IDE、源索引／overlay、扩展库、标量类或 DLL response-file
重构。混合文件只暂存本批必要 hunk。`freeze_stdlib_source.py` 校验 index
blob 与导出字节，并通过单次 `cat-file --batch` 避免逐文件启动 Git。

`stdlib53-b142-source-win-dev6` 从头编译 251 TU，Linux 对应
`stdlib53-b142-source-linux-dev6` 从头编译 250 TU，包括真实 XRT、所有
编译器／API／原生 provider；无假 provider 或对象覆盖。仅复用 B138/B140
真正重建候选库的 vendor TCC。新 Thread provider 编译无警告豁免。

`tools/check_stdlib_thread_language.py` 的最终报告目录为：

| 平台 | `build/test/` 下目录 | 阶段 |
| --- | --- | ---: |
| Windows | stdlib53-b142-thread-language-win-dev4 | 140 |
| Linux | stdlib53-b142-thread-language-linux-dev4 | 140 |

两份最终报告均要求 `Complete=true`、`InputsUnchanged=true`、Errors 为空。
每次使用全新输出目录；前轮失败／中断证据保留，不改写为通过。验收包括：

- 13 个语言用例：确定性 C、真实 JIT、严格 O0/O2 AOT，init/main/fini 的
  分配、释放和错误平衡；Thread 闭包捕获精确 NUL string、bytes、char。
- 实际 TCC relocation、源码 image 退役后的平衡；Windows 额外通过
  VirtualQuery 确认 image 地址 MEM_FREE。Linux 保持候选 SO 常驻。
- 实际 Typed/Lowered IR 的六种操作、borrow/owned/error ABI、闭包捕获，
  生成 C 调用数量和每次完整 TypeValueId；不存在旧 Thread 函数体。
- O0/O2 的实际 IR 篡改拒绝：resident 用例 103 次、容器用例 180 次、
  create OOM 用例 25 次、policy OOM 用例 19 次、stack OOM 用例 44 次。
  每次同时验证 IR 拒绝、Cgen 保留 KEEP 和产物计划为空。
- 三个语言 OOM 的完整当前线程分配前缀：create 8、policy 8、stack 13；
  checked 布尔失败及退出码 1，不用诊断指针代替成功通道。
- 原生策略真实生命周期：同身份伪造句柄（含无效 payload 地址）与标量
  拒绝；准入无分配且保留先前错误；策略独立于已销毁源容器；nullable
  默认和实际元数据构造 OOM 的 4 个前缀。
- 10 个语义／编译负例均保留原输出，包括普通复制、投影借用复制、
  未证明非空的 receiver、错误入口签名及方法／构造参数。
- 既有 Thread ABI、Thread/Sync 默认及运行用例，Sem/Sync 容器回归。

生成 C 仍有既有未使用静态 helper 的显式警告豁免；compiler-internal
IR 宿主因已有 `bytes` typedef shadow 不声称额外 Wshadow。新原生策略和
JIT 宿主没有这些豁免。OOM 是当前线程逐分配点注入，不冒充跨线程注入。

## 后续验收

本批不是新 shipping DLL/SO/VFS、独立静态链接裁剪、全仓 RC0 或 53 模块
总验收。旧父门的 Thread C 预期已更新；其 release CLI 端到端重跑仍须使用
下一批真正重建的产品，不能用未更新的主 release 偷换本批源码证明。
XRT Wait 是逻辑完成，不是 OS join；本批不声称动态卸载 XRT image 安全或
Linux terminal TLS 退役完成。仍需继续其他残存生成式资源拥有图、真实产品
与跨组验收。19 个未决策分组不变，对象池最后，长期目标保持 active。
