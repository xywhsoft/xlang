# Dial 受管资源底座（尚未发布语言入口）

本批导入 XRT `dbd5f71c22d706faddb284e9507bd13c5ead4baa` 的原生 Dial 拥有图和地址列表连接能力，
增加 xlang 驻留资源载体。它不是公开 `tcp.dialAsync`、代理或整个网络分类的完成声明。

## 原生快照

只导入 `tcp.h`、内部 TCP 声明、TCP 拥有适配器、Dial 和 Dial Future 五个核心节的增量。
两个实现快照及两个声明头保持一致，增加 15 个公开 C API；链接登记由实际预处理闭包生成。
没有混入原生仓库未提交的文件／内存改动。

固定快照继续采用 `uint64` 微秒／`xdeadline` ABI，未导入上游的毫秒时间布局。
Dial 的超时／回退默认值、Deadline 字段、时钟构造与调度、无符号配置校验保持原快照语义。
原生算法、候选排序／竞争、Resolver、取消及完成逻辑全部复用 XRT，没有新增语言层竞争算法。

## 受管载体

- `xl6ExecutionTcpPrepareDialV1(source, engine, resolver?)` 仅保留真实父对象，不提交连接。
  非空 Resolver 选择主机路径；空 Resolver 选择直接地址列表路径。没有隐式默认 Engine／Resolver。
- 主机和不可变地址列表分别使用独立 Source 方法，返回原生 Future 的一个真实引用。
  载体保留原始 Future 直到提升或真实终态；重复启动、错误载体类型、错误路径均 checked 拒绝。
- `xl6ExecutionTcpPromoteDialV1` 将同一个载体中的成功结果提升为实际 Stream，释放待定 Future 与 Resolver 父引用，继续保留 Engine。
  载体不是伪造的 xnetstream，未提升时不能调用 Stream 方法。
- 最后引用退役只请求取消，不把取消请求伪装为终态。若完成竞争交付了未观察的连接，先 Abort 再释放；不会依赖最终 Engine 销毁来掩盖孤儿连接。
- Accept 和 Dial 共享同一套待定结果清理逻辑，但保留明确且不可混用的操作种类。
  通用 Resource ABI 不变，只有私有 Stream 具体扩展记录增加字段。

`NetDialNative` 是独立的显式拥有域，登记实际 Engine runtime、Resolver 请求、Dial Done、
Future producer、CancelWatch、Stream payload 和静止能力。基础 TCP／Engine 不因此选入 Dial 或 Resolver。
未知策略仍拒绝；不把它们当作不透明叶子，不用推算的资源数量伪造引用计数。

## 验收与边界

入口 `tools/check_stdlib_tcp_dial_runtime.py` 编译实际固定快照和实际组件闭包，覆盖：

- 原生 Dial/Future 完整拥有图、真实 Resolver 阻塞回调、候选连接、各清理顺序和构造失败；
- 受管待定／成功／未启动／带 Resolver 父对象的图、提升、取消退役、未交付结果清理；
- 原有 TCP／Accept 图及延迟完成回归、真实 NUL 字节往返、分配失败后重试；
- 入队后、Worker 尚未接管时的真实 BUSY 边界；完整图证明在实际静止后重新取得；
- 从实际封闭 catalog 中移除四种策略时的确定性登记拒绝对照。该观察器不授予图能力；原生夹具另行检查策略身份，受管夹具另行完成真实图回收。

GCC／TCC、O0／O2，select 及 Windows IOCP／Linux epoll；Linux GCC 额外运行 ASan／UBSan。
测试只使用离线数据和自有字面量 loopback，不做 DNS、公网、扫描、畸形协议或网络安全专项测试。
证据保存在 `F:/xlang-validation/five-groups-20261021/p3-dial-import`；失败／中断轮次保留，只有冻结输入的终态通过报告计为通过。

仍需完成公开语言类的预建结果投影、固定就绪队列 Future 合同、同步及期限／取消组合、代理控制器。
受管主机 Source 本批未进行系统 DNS 测试；主机原生路径由自定义固定地址 Resolver 夹具覆盖。
完整源码构建与基础 TCP JIT／模块／AOT 回归不等同于 Dial 语言入口验收，更不等同于 shipping DLL／SO／VFS 发布验收。
后续聚合 Server、向量／文件／帧、UDP、非对称密钥、X.509、TLS 和联合验收仍在原长期任务范围内。
未决策状态不变：CRT 0，XRT 11 组。
