# 冷启动与哈希定义复核

## 结论与范围

本轮未确认新的包内算法、错误分类或所有权 BUG，因此没有修改
`imagehash_native.c`、公开 C/XLang API、stb 依赖或冻结 XRT。
补的是先前预热测试不能证明的首次调用、独立算法参照和双语应用范例。
新增测试夹具曾误用 Buffer 参数、PowerShell 条件优先级和 println
分隔空格；均在夹具中修正，保留失败现场，不计为产品缺陷。

## 新增边界

- `test/test_cold_state.c` 每个 kind/budget 启动新进程，覆盖三种哈希、
  空路径、非法 hamming 和普通 valid 查询。记录实际注入与 XRT 原因，
  清除注入后重复相同输入，再调用成功操作。独占夹具最后释放私有
  线程键，核对活动分配数、字节数、非法释放、重复释放与 UAF 计数。
  私有键只在测试进程销毁，没有增加产品公开析构接口。
- 解码器可恢复局部分配失败；格式查询也不必因为未能准备可选诊断
  而失败。测试允许正确且无错误的成功，不把“触发注入”等同于
  “公开操作必失败”。真正失败的哈希仍须保留 XRT 内存原因。
- `test/cold_start.xl` 对三种哈希逐预算启动新 JIT/AOT 进程；失败只
  接受实际注入导致的 MemoryError，取消注入后核对值与诊断恢复。
- `test/hash_vectors.c` 生成四种尺寸与五种灰度图案，共 20 张 PGM，
  输出真实缩放像素、三种公开哈希及相邻 pHash 距离。
  `test/check_hash_vectors.py` 用直接二维定点 DCT、均值和相邻比较的
  数学定义核对 60 个哈希与 20 个距离。另检查原尺寸像素、纯色缩放，
  并拒绝三种哈希的位翻转对照。它不调用包内部算法，但缩放像素仍
  来自被测 stb 依赖，不能据此宣称独立认证全部缩放算法或图片格式。
- 使用实际 cgen 的八个 extern 原型，与公开 C 头共同交给严格 GCC；
  故意错改拥有型返回的负例必须拒绝。原有三条直接接管且不复制
  正文的检查继续运行，不声称路径检查/长度扫描也没有成本。

## 平台与执行证据

| 范围 | 最终现场 |
| --- | --- |
| Windows 默认完整门禁 | 246 项符合预期，含 6 个必须非零退出的负例；89 个输入/工具指纹保持 |
| Windows 冷调用 | C 原生 60 个预算进程；另六个 TCC JIT 冷入口；XLang JIT/AOT 各 57 个预算进程 |
| Linux 原生快照 | 84 个复制源与 GCC/cc1/Python 三工具前后不变，ASan/UBSan 保持开启 |
| Linux 冷调用 | C sanitizer 103 个预算进程；另六个 TCC JIT 冷入口；XLang JIT/AOT 各 54 个预算进程 |
| 三种 C 引擎 | GCC、TCC JIT、TCC AOT 各核对完整 20 张图的数学定义与损坏结果对照 |
| 独立程序 | 两平台各 11 份 AOT 程序，新旧双语输出及原线程/分配边界通过 |
| 门禁故障控制 | Windows 静默编译器 raw37/outer1；Linux raw37/127，保留命令、原始码、指纹与现场 |

Windows 普通构建与 Linux sanitizer 的分配数量不可直接比较，后者继续
保留 stb 的 `STBIR__SEPARATE_ALLOCATIONS`；原预热预算未改。本轮没有
吞吐/峰值内存基准，不作新的性能承诺。

本机可复查证据：

- `build/test/imagehash_audit_1793354039/`：首轮探测、完整门禁包装结果、
  故障控制与八文件冻结指纹。
- Windows 完整外部报告：
  `build/test/imagehash_guard_30972ad9538d4b03afb6842bde6eb288.report.json`。
- Linux 最终原生快照：`/var/tmp/xlang-imagehash-audit-gYOy1SYD`，
  `gate.exit` 为 0，完整日志与源/工具指纹校验保留。
  外层控制脚本首次只在 stderr 搜索错误，遗漏了门禁刻意合并到日志
  的 stdout；随后独立复验真实 37/127 控制成功，不重签为产品修复。

门禁之后仅更新说明与详情源，不重复运行运行时矩阵。八个冻结文件
保持不变；普通包总表仍为 G30/R7/M59/X4，不把本阶段算作新增发布包。

## 双语范例

`examples/similarity.xl` / `similarity_c.c` 使用同一 pHash、valid 和
hamming 接口比较图片，提供固定输出与说明。默认两张图片和传入
同一图片分别检查不同/相同结果；缺失第一张图片必须失败且保留原
图片错误，不能继续成功调用后覆盖诊断。GCC、TCC JIT/AOT 与 XLang
JIT/AOT 同时覆盖。阈值只是业务范例，不用于安全认证或图像身份判定。
