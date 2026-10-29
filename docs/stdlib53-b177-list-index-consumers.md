# List 动态索引与容器查询

原生 List 现在可供开放 `var` 的索引读取、索引写入和 `in` 查询使用。
这些入口复用真实 typed List，保留完整 int64 键和精确文本长度，不将
任意 HANDLE 当成 Array。静态索引已由后续
[List 静态索引合同](stdlib53-b178-list-static-index.md) 迁移；源 List
工厂仍采用旧 IntMap 载体，不能据此宣布整个 List 迁移完成。

## 索引与错误合同

读取返回独立拥有的 Value，缺键返回正常 null；操作失败返回 C NULL
并使 checked scope 失败。nullable string 的 null 与空字符串不同，
char、uint64 和 bytes 保留各自域，不靠有符号整数或 C 字符串转换。
写入在所有结果下恰好消费一次输入。游标、BUSY owner、未绑定投影
及伪造类型身份不能绕过真实驻留协议。

动态对象写入先保留 receiver 和旧子对象，使用现有 SetTake 发布新值，
记录操作结果后再释放输入、旧值和 receiver。旧值析构可以观察、修改
已经提交的对象，或释放其独立拥有的回指；它不在 receiver 的 BUSY
保护区内执行。失败时清理错误不能覆盖原始错误，包括没有诊断对象
的失败。普通替换只增加引用操作，不增加字段快照或内存分配。

## 组件裁剪

动态索引的读、写实现从 Array 专属翻译单元中移出，分别注册独立
驻留组件。旧声明头只转发新的声明头，不包含实现，也不形成二进制
依赖。已知容器的查询按已验证的 XLIR 类型选择入口：

| 已知来源 | 查询入口 | 排除的驻留组件 |
| --- | --- | --- |
| Array | ArrayMethodContains | List、动态索引和通用 var 查询 |
| List | ListValueContains | 不以 Array 协议猜测载体 |
| Dict 或 Set | ValueWireContains | Array、List 和动态索引 |
| 开放 var | ValueContains | 保留实现其开放语义必需的家族 |

Cgen 与依赖选择共用同一物理入口选择函数，没有增加源 AST 搜索或
内联容器算法。wire IntMap 分支是明确的迁移边界，不是原生 List。

## 验收结果

Windows 重新编译 263 个翻译单元，Linux 重新编译 262 个。冻结候选为
`E:/xlang-validation/stdlib53-b177-source-dev4/frozen/demo6` 和
`/var/tmp/xlang-stdlib53-b177-source-dev4/demo6`。编译器、XRT 和驻留
实现均从源码构建，仅保留已记录的真实供应商 TCC 引擎。

双端各通过以下专项：

| 专项 | 通过阶段 |
| --- | ---: |
| 动态索引与查询，含原生独立编译和裁剪产物运行 | 166 |
| 存量拥有值索引及 continuation | 27 |
| 原生 Array 拥有读取 | 260 |
| Array 读写与失败清理边界 | 70 |
| 细化键合同 | 33 |
| List 快照迭代 | 64 |
| 存量容器循环 | 64 |
| List 方法及 continuation | 75 加 9 |
| Closed/Open 恢复及整批分配失败前缀 | 96 |

索引专项包含 24 条闭合故障前缀，共 74 次尝试，每次独立检查账本和
finally。三个真实驻留实现负对照先编译，再由相同合同拒绝。存量
索引专项拒绝 483 项损坏 IR，在输出 C 和头文件之前阻断；模块完整
OOM 前缀按单进程批处理，O0/O2 各运行 113 个点。

纯 Array 与 Dict/Set 程序不仅检查实际 artifact plan，还仅编译其
声明的原生模块和驻留源文件，在 O0/O2 链接、运行并关闭账本。此处
MemoryDebug 是独立的测试扩展，不是程序的生产组件根。

完整驻留目录双端通过 224 组件、1,206 精确导出和 13 项负对照。
Linux ASan/UBSan 覆盖选定 List 及 Array 原生闭包；不扩展为整个
编译器或全部生成程序的 sanitizer 认证。

最终专项报告为 `E:/xlang-validation/stdlib53-b177-index-consumers-win-final6/results.json`
及 `/var/tmp/xlang-stdlib53-b177-index-consumers-linux-final6/results.json`。
测试宿主、合同和 runner 的修正使用显式 test-root 与实际输入哈希，
没有替换冻结的生产源码。先前失败记录保留，不计入通过阶段。

文档索引仍有 14 项历史拒绝，本批新增为零；这不是整个文档门已通过。

## 后续工作

List 源工厂、类型准入及转换、clone、合并和恢复仍需按统一载体
合同联动切换。Set/Dict 迁移、发布 DLL/SO、VFS、打包 AOT 和三组
总验收也未完成。以上专项不关闭整个长期任务。

共享默认子对象继续按声明 clone 能力隔离，否则 checked 拒绝，
不修改外部共享原对象。CRT 未决策为零，19 个 XRT 未决策组及对象池
最后的安排不变，见 [标准库决策清单](标准库决策清单.md)。
