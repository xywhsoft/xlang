# B133：字段发布后退休与真实语言对象图验收

本批修复的是语言所有权问题，不增加未决策的标准库功能，也不宣布 53 个模块全部封板。

## 原因与合同

真实语言 fixture 在 Windows、Linux 上均发现：将 `marker.children` 从包含两个自身
引用的 Array 替换成新 Array 时，旧 Array 的两个 owning slot 被释放，但父对象仍在
`xrtValueObjectSetNew` 的 BUSY 区间。两个合法的父对象 release 被拒绝，残留引用再经
代码租约保活整个模块。被替换的 Array 已不在模块图中，退休器不能将这些计数当成内部边。

XRT 的防重入约束没有放宽。新增 `xrtValueObjectSetNewPostCommit`：

- 新值在成功、失败两条路径都被消费；失败不改变字段。
- 在一次 ownership mutation 内固定接收者，分离 COW，移交旧字段并发布新字段。
- 结束自己的 mutation/BUSY 区间后释放旧字段，允许析构读取已提交字段及释放自己拥有的反向引用。
- 最后释放真实接收者 pin；保留第一份错误，后续清理错误不能覆盖它。
- 保持首次插入顺序、精确字节键、相同值引用消费、原有 Set/SetTake/SetNew 的合同。

上游 XRT 提交：`9c66e301`。只同步 `include/xrt/value.h` 和
`src/value/value_container.c` 的明确变更片段到 XRT/xruntime 两组快照，未导入扩展库变化。

XLIR 已明确的拥有式字段写入映射到这一个原生调用；声明／字符串键、闭包字段和
动态索引的 Object 分支采用相同的发布后退休规则。Cgen 不增加对象图推断、重试或
特例析构器。既有消费合同、异常边和外部调用错误范围保持不变。

## 当前验收证据

证据统一放在 `E:\xlang-validation`，失败批次保留，没有改低析构期待值。

| 门禁 | Windows | Linux |
| --- | --- | --- |
| 真实公共编译 API／JIT／模块退休 | `stdlib53-b133-graph-win2/ownership.json`：44 阶段、38 模式 | `stdlib53-b133-graph-linux2/ownership.json`：44 阶段、38 模式 |
| Queue／Tree 语言层完整调用线程 OOM 前缀 | `stdlib53-b133-language-oom-win2/language-oom.json`：992 阶段、10 条完整前缀 | `stdlib53-b133-language-oom-linux1/language-oom.json`：992 阶段、10 条完整前缀 |

两个对象图门禁均为 `Complete=true`、`InputsUnchanged=true`，O0/O2 各覆盖：
SPSC、MPSC、MPMC、Array、Future、闭包六个实际语言循环，以及外部 Class、容器和
独立动态 Value 外壳保活。每个模式测量 25 次实际编译／JIT／退休，共 1,900 次测量
生命周期（另有预热）。字段完整、析构恰好一次、外部根释放后退休、分配／释放平衡，
并检查非法释放、重复释放及释放后使用计数。

语言 OOM 首次不命中前缀仍为 SPSC 10、MPSC 21、MPMC 19、owned Queue 192、Tree
projection 233，均包含 O0/O2。失败采用 checked 调用的明确结果见证，不假定 OOM
一定能分配错误对象；检查完整 owned 返回值清理、重复 fini 和真实分配账本。

上游 `stdlib53-b133-xrt-postcommit-win1/post-commit.json` 的六条 native/single
GCC O0/O2/TCC 路径也为 Complete、InputsUnchanged；逐条验证 100 次实际反向引用
替换、接收者最后 pin、COW、顺序、NUL 键、重复引用消费、第一错误和六个插入／COW
OOM 前缀。现有 Value/容器防重入测试同时复测，原合同没有改成允许 BUSY release。

## 尚未由本批证明的范围

- Windows 使用同一套当前源码构建的 resident 对象；不宣称重建了仓库内全部发布 DLL/VFS。
- 不等于 AOT 对象图退休、Windows MEM_FREE/unmap 或任意第三方 ownership adapter 验收。
- 三次真实 await 的 Queue continuation 验收仍待完成；调用线程 OOM 不等于调度器全线程 OOM。
- 整个 AvlTree 的常驻 owner／方法效果／对象图迁移、旧总门禁整理及 53 模块发布级汇总仍待完成。
- 对象池仍留在最后，不扩大未批准的功能范围。
