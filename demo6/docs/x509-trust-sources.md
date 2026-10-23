# X.509 信任来源

TrustStore 的所有来源共用现有稳定版本／快照拥有模型；只导入调用方明确
选择的信任锚，不自动联网、不下载中间证书、不修改操作系统根证书库。
导入不等于验证证书的签名、路径或远端身份，后续仍须显式执行验证。

## 公开面

| 接口 | 输入与结果 |
|---|---|
| `store.addEncoded(data, size=-1)` | string／bytes／Buffer，识别单张 DER 或 PEM 集合；返回新增数 |
| `store.addPem(text, size=-1)` | 保留显式 PEM 入口 |
| `store.addFile(path, maxInputBytes=67108864)` | 原生文件路径，限额读取完整输入后导入 |
| `store.addVfs(filesystem, path, maxInputBytes=67108864)` | 显式 Vfs 名字空间，不落临时文件 |
| `store.addSystem()` | 从原生 OS 根来源取得独立版本，完整成功后合并 |
| `TrustStore.fromFile(path, maxInputBytes=67108864)` | 普通语言工厂，返回独立可变信任库 |
| `TrustStore.fromVfs(filesystem, path, maxInputBytes=67108864)` | 同上，来源为 VFS |
| `TrustStore.fromSystem()` | 直接接管原生系统信任库，无额外 DER 克隆 |
| `TrustStoreSnapshot.fromSystem()` | 直接取得不可变版本，不引入可变外壳／互斥锁 |

`bytes` 参数通过现有 `cbytes` 只读调用视图传递，保留完整逻辑字节长度。
不会经过 strlen、临时 Buffer 或逐元素装箱。Buffer 直接借用原生片段，
调用返回后不保留任何来源地址；并发修改同一个 Buffer 仍须由调用者同步。
文件／VFS 按原始字节读取，不做文本编码自动转换，避免改变 DER 内容。

## 长度与错误

- `size=-1` 为完整输入；非负值按实际长度截断，超过实际长度则取实际长度；
  其他负值为 RangeError。这是编码数据的显式前缀语义。
- 文件／VFS 的 `maxInputBytes` 则是硬限额，不是前缀。超出即 checked 失败，
  绝不把截断的数据当作完整证书。默认 64 MiB，零为零字节，负值拒绝。
- 路径检查完整逻辑长度，内嵌 NUL 为 ValueError，不允许 C 字符串静默截断。
- 文件、VFS、解析及系统来源保留实际原生错误；不按诊断文本猜测格式或失败种类。
- 重复证书去重，重复-only 成功返回零。空编码内容为格式错误，不是空信任库。
- 整次导入失败不发布新增计数或条目；此前的快照和分离子证书不受影响。

## 实现边界

内存与文件的 DER／PEM 格式识别共用 XRT `xrtX509StoreAddData`，算法与
事务复用已有 Add／AddPem。首字节 DER SEQUENCE 优先；否则按行首 PEM
BEGIN 边界选择格式，不以一次解析失败作为再次尝试其他格式的依据。

文件／VFS 先在原生硬限额内读入，再取得目标 TrustStore 的私有锁。系统
枚举也在锁外完成。持有旧版本时沿用既有 COW；没有额外快照拥有者时原位
更新。没有第二套信任对象、临时证书库合并或 Cgen 生成的导入实现。
来源 API 是同步的，效果合同不标记 pure／no-raise／nonblocking；异步工作线程
不因此获得任意同步 I/O 都不会阻塞的承诺。

系统来源只在显式调用时使用：Windows 读取用户／机器 ROOT，POSIX 使用
原生受支持的本地根文件／目录，遵循 XRT 的来源选择及错误合同；不使用
系统链引擎联网发现。系统根可能随管理员操作改变，取得后的版本仍保持稳定。

按功能分为 data、file、VFS、system 与 mutable-system 驻留组件。仅使用
基础信任库或编码输入不会自动链接文件、VFS、系统根来源或 TLS；声明层的
Buffer／Vfs 依赖并不表示输出程序必须带入其实现。

独立 XRT 提交 `e52d91421`。定点回补记录见
[x509_store_input.json](../lib/xrt/backports/x509_store_input.json)，保留嵌入快照
微秒／Until 与独立 XRT 毫秒／For 各自合同，不进行整体时间 ABI 升级。

本专项不代表身份匹配、TLS 或 shipping 验收已经完成。
