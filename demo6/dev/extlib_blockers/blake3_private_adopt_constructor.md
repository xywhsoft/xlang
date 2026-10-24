# blake3 导出类的私有句柄构造器仍可被包外调用

状态：blocked-upstream。普通扩展库任务不修改 XLang 编译器。

## 最小复现

当前 `release/xl.exe` SHA-256：`616888E9DA0097E5449CB4ABD25FC42F87606D836611B3D739860CE57E2D1E3F`。
在 `demo6` 目录仅执行编译检查，绝不运行负例：

```text
release\xl.exe check release/extlibs/blake3/test/private_adopt_constructor_negative.xl
```

导出类 `Blake3Hasher` 与 `Blake3KeyedHasher` 的 `create(point handle)`
均已声明为 `private extern`。包外的两次 `Class(null)` 仍返回 `ok`、
退出码 0；预期是访问权限错误。`null` 仅用于安全证明可达性。
薄绑定 `xblake3_hasher_adopt` 原样返回指针；伪造非空指针进入拥有型
对象后，析构可能访问或释放不属于自己的内存。

对照：`release\xl.exe run release/examples/extlibs/blake3_basic.xl`
正常运行，普通工厂、clone、reset、keyed、KDF/XOF 用法未受私有标记影响。

## 修复建议与复验

在主线语义和构造器重载解析中落实导出类 `private extern create`：
包内 clone 等成员可接管句柄，包外构造调用必须报访问权限错误。
修复后先要求本负例非零退出且诊断明确为访问控制，再复跑本包
Windows/Linux C、XLang、双语范例及完整 AOT 门禁。不能通过运行
任意非空指针探针验证，也不应为了规避问题取消导出哈希器类型。

2026-10-20 包侧已另行移除公开 keyed 构造器的 `point + keyLen` 入口，
改为 `bytes` 值与真实长度校验；它不消除本文件描述的私有接管缺陷。

另需在包层继续审计公开的 `*Native` 成员；它们不应作为导入方的
裸指针接口。在构造器主线缺陷修复前，本包不签收发布级。

2026-10-21 复核：在 `demo6` 根目录仅执行上述 `xl check`，仍输出
`ok`、退出码 0；未运行危险负例。完整教学范例 JIT 与 Windows
`-SkipAot`、Linux 包门禁通过不代表此访问边界已修复。

2026-10-24 当前发布版复验：`release/xl.exe` SHA-256 为
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`。
仅运行 `xl check release/extlibs/blake3/test/private_adopt_constructor_negative.xl`，
仍输出 `ok`、退出 0；未执行不安全的原生句柄构造。需由主线修复访问
控制后再验收，不在扩展包中增设兼容包装掩盖可达性。
