# 2026-11-07 范例失败修复

本轮处理用户列出的 44 个范例，Redis 服务环境项不在范围内。单人开发，
保留工作区其他 IDE/语义补全改动；未修改 XRT，也未进行外网或攻击性质测试。

## 根因与处理

| 分组 | 处理 |
| --- | --- |
| 17 个 array/var 范例 | 语言容器使用受管原生存储，旧 C 接口读取 XRT ARRAY/OBJECT。通过已有 `xl6ForeignDataSnapshotV1` 显式投影；修正 xregex/tar 的 C 边界声明；Socks 支持 bytes。CSV 另修复嵌套数组插入缺少目标元素类型的问题。 |
| tutorial/09_class_lifecycle | 只读接收者绑定使用借用；析构内调用 self/base 方法不再获取 FINALIZING 对象的强引用。普通调用仍保留调用方的接收者保护，闭包捕获继续拥有引用。 |
| 旧扩展与标准库范例 | 迁移 Buffer、毫秒时间、bytes 进程输出、ProcessExitStatus；xbot 使用可作为目录名的数值时间 ID，file.size 保持 uint64；Memcached 使用 Buffer 并校正长度和 items 结构。 |
| MQTT / Modbus | MQTT 使用通用受管 TCP 调用期视图，原生 XRT MQTT 算法、累计期限及取消规则保留。Modbus 使用受管 TCP、精确 MBAP 分帧、事务/单元/功能/写回校验，C 整数 ABI 使用 int64_t。 |
| Volc | 删除未使用的 xhttp.client 语言导入，保留实际 C 依赖；登记 Future 原生依赖，补齐 Future bridge 的 FUTURE_CONTINUE 裁剪依赖。自有字符串通过冷路径分配转交适配器接管。STS 示例改成确定性的离线示例。 |
| 9 个游戏 | 迁移到 XGE3 直接声明。初始化/纹理绘制使用栈描述符映射；补齐 Font.drawText、受管 Sound，保留素材和玩法。资源在首帧开始后创建，退出时先解除回调，再清理资源，再关闭引擎。 |
| 3 个日志范例 | 分开校验原生生产者错误 ABI 与 Buffer 受管访问的准备错误 ABI。 |
| stdlib_api_contracts | 不再要求已裁掉的不可达函数持有运行时持久化执行计划；可达函数及结构校验不放宽。 |
| C 回调声明 | 生成的共享错误结果类型遵守同一头文件保护，避免与配套 SDK 重复定义。 |
| CSV 全局 AOT 退出 | 旧匿名 native handle 没有完整退役合同；Reader/Writer 改为组合显式 owned view。原生字段操作直接调用 C，保留公开 Reader/Writer 行为。 |

## 必要成本

- 旧的 XRT wire-value 接口会复制/投影容器数据，本轮没有声称零拷贝。
- `xl6ForeignTcpVisitV1` 只借出调用期指针，保持真实拥有者和活动调用保护；
  不增加协程栈，不恢复可逃逸的 raw handle API。独立 FOREIGN_TCP 组件避免把 TLS 拉入 MQTT。
- CSV 的 C 指针工厂通过五个冷路径 status/out 适配器交付拥有权；普通 C 工厂仍保留。
  方法有受管视图访问检查，公开语言 Reader/Writer 组合层有拥有值读取成本。
- Volc 冷路径适配器将线程结果槽的 XRT 分配转交为 @owned string；不额外复制载荷。
- XGE 描述符使用栈记录；Sound 有一次资源控制记录及 native record 分配。
  没有逐帧方法转发 C 包装层，但仍有正常的语言生命周期与调用准备成本。

## 验收口径

Windows：44 个范例通过 check、AOT 构建/运行；35 个非游戏范例通过 JIT，
9 个游戏另经默认动态后端 JIT 限帧运行。游戏 JIT 与静态 AOT 都创建真实窗口，
每例运行 3 帧；不等同于完整人工玩法验收。shooter 静态 EXE 的导入表不含 xge3_host.dll。

3 个核心回归覆盖析构中的 self/base 调用与字段修改、调用参数重绑全局接收者、
嵌套 string/uint64 数组及拒绝非法类型；CSV 的全局生命周期、边界/OOM、文件和 BOM
四组专项均通过 Windows JIT/AOT。

Linux：在隔离构建目录使用同一套 33 个可移植源码包，验证 35 个非 GUI 范例的
check/JIT/AOT 和 3 个核心回归。发现的 Modbus LP64 原型差异经 int64_t 修正后单项复验。
Linux 当前本地运行包只包含原有精选包，本轮没有把整个扩展库目录额外复制进发布目录。

实时 Volc 范例在去掉 VOLC_* 凭据的环境执行，必须退出 2；只证明无凭据保护路径。
不代表云端服务验收。Redis 继续按用户要求不处理。

工具：`tools/check_example_failure_regressions.py`；精确命令、哈希和日志见本轮
`dev/release-materials/provenance/example-failures-20261107/`，源码增量见对应 sources 目录。
历史失败报告保留原字节，不用新结果改写旧证据。
