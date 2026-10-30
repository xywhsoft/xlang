# blake2 导出类的私有句柄构造器仍可被包外调用

状态：blocked-upstream。普通扩展库任务不修改 XLang 编译器。

2026-10-30固定发布物复验：当前基线见根目录docs下的发布验收矩阵。
两平台仅check负例仍输出ok/退出0，默认门禁将它记为失败，绝不执行。
同次包侧首错修复、216答案、C/语言/双语与普通AOT通过，也不覆盖本缺陷。
Windows63阶段和Linux59阶段另有合法顶层对象的AOT退役失败，分别登记，
不推断两者同根。对应新局部生命周期控制三阶段均0，主线/XRT均未改。

## 最小复现

早期记录的 `release/xl.exe` SHA-256：`616888E9DA0097E5449CB4ABD25FC42F87606D836611B3D739860CE57E2D1E3F`。
从当前仓库根目录仅执行编译检查，绝不运行负例：

```text
release\xl.exe check release/extlibs/blake2/test/private_adopt_constructor_negative.xl
```

四个导出类 `Blake2bHasher`、`Blake2bKeyedHasher`、`Blake2sHasher`、
`Blake2sKeyedHasher` 的 `create(point handle)` 均已声明为 `private extern`。
包外用 `null` 调用四个构造器，`xl check` 仍返回 `ok`、退出码 0；
预期是访问权限错误。`null` 只用于安全证明可达性，不执行此文件。
薄绑定的 `xblake2b/s_hasher_adopt` 原样返回指针；若传入伪造非空
指针，对象析构可能访问或释放不属于自己的内存。

曾有独立的包侧构造边界：两个导出的 keyed 类公开
`create(outLen, point key, keyLen, maxInput)`，包外可把 1 字节 Buffer
指针与 64 字节长度一起传入。2026-10-20 包侧改为从 `bytes` 值读取
真实长度，`dev/extlib_blockers/blake2_keyed_pointer_length.xl` 现仅作
**只编译、绝不可运行**的回归负例，预期 `create` 重载错误。
此修复不消除本文件描述的私有 `adopt` 构造器缺陷。

对照：`release\xl.exe run release/examples/extlibs/blake2_basic.xl`
正常运行，工厂、clone、reset、keyed 和 Buffer 用法未因私有标记失效。

## 修复建议与复验

主线语义和构造器重载解析必须对导出类落实 `private extern create`：
包内成员可用，包外构造调用必须报访问权限错误。修复后要求本负例
非零退出且诊断明确为访问控制，再复跑本包 Windows/Linux C、XLang、
双语范例和完整 AOT 门禁。不能通过检查裸指针是否“有效”来修补，
也不应为规避问题取消导出四个公开哈希器类型。
keyed 类的原始指针/长度构造器已从 XLang 导出中移除，b/s 两条
只编译负例已纳入包级门禁；C 原始 ABI 仍由 C 调用方负责配对。

2026-10-21 复核：在 `demo6` 根目录仅执行上述 `xl check`，仍输出
`ok`、退出码 0；未运行危险负例。完整教学范例 JIT 与 Windows
`-SkipAot`、Linux 包门禁通过不代表此访问边界已修复。

2026-10-26 当前发布物复验：Windows `release/xl.exe` SHA-256 为
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`，
Linux `release/xl` SHA-256 为
`feaadad13ae1929547d4d9e8a8d5c67bb4e7a4dcd841a13700d66e43fd1732c5`。
两平台 `xl check release/extlibs/blake2/test/private_adopt_constructor_negative.xl`
均输出 `ok`、退出 0。Windows 正例门禁与 Linux C ASan/UBSan、
六条独立 AOT 均通过，旧 AOT 缺符号不再复现，但不能覆盖本访问缺陷。
默认 Windows/Linux 包门禁现把这条只编译负例放在正例之后；未拒绝
私有句柄构造器时必须非零退出，禁止误签收。负例绝不运行。
