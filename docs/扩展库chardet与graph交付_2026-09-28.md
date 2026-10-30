# chardet / graph 交付与限定验证

本记录是全生态长期压实任务中的两个包进展，不是全生态完成证明，也不把剩余语言缺陷
从验收范围中删除。使用固定构建，不升级 XRT，不替换正式 CLI/DLL。

## 本轮完成

| 包 | 本轮交付 | 验证 |
| --- | --- | --- |
| chardet | 五项 API 的文本/Buffer 合同、C 所有权、编码猜测与有效性区别、README、官网正文及目录链接 | Windows 34 项；Linux 独立 C；文档 4 个代码块 |
| graph | 九项 API 的有向性、返回排序、权重、错误、资源与性能合同，README、官网正文及新目录入口；补 DLL 复用门禁 | Windows 38 项；Linux 独立 C；文档 4 个代码块 |

两包既有 C/xlang 基础范例覆盖全部公开操作，共享固定黄金输出。
官网由包内 docs/site.html 生成到本地官网，源码与目标哈希一致；没有声称已部署到外部服务。
目录现在有 211 个条目，205 个 C 混编、6 个纯 XL；这是页面条目数量，不是公共包总数或准入数量。

两项门禁纳入 `test/suites/12_phase1_extlibs.bat`，同时可单独运行，避免每次补库都必须跑全生态。
未运行完整 phase1 套件，已知其他包失败不在本次修复范围内。

## chardet 证据

- Windows 34 项全部通过，10 个生成 C 原型与公开头严格编译相容。
- 22 组原始字节覆盖空串、ASCII、中文、五种 BOM、仅 BOM、BOM 后非法字节、无 BOM 的
  UTF-16/32、NUL、截断、过长编码、代理项和超出 Unicode 范围的码点。
- 原生检查不分配内存；失败注入未触发分配，live 数与非法释放计数保持不变。
- 四线程 8000 轮检查借用名称、不可变输入及线程错误状态。
- JIT/AOT、独立 GCC、C runner、两个 DLL 的同一上下文三次运行通过。
- Linux C 使用 GCC、ASan/UBSan、泄漏检查和 LP64 原型核对；范例输出一致。
- README 两段和官网两段 XL 代码实际运行，输出与文档一致。

## graph 证据

- Windows 38 项全部通过，增加 xl.dll / xlc.dll 宿主构建和上下文复用四项。
- 128 组独立参照图、容量边界、Unicode/NUL C 名称、输入和结果寿命通过。
- 九项 API 共 218 个分配失败点通过，失败清理后没有泄漏、双重释放或非法释放。
- 四线程共 1000 轮，共享只读输入、独立算法状态及线程错误状态通过。
- 九个生成原型与 C 头相容；C/XL JIT/AOT、独立 GCC 范例输出一致。
- Linux C 的 ASan/UBSan、泄漏和 LP64 原型检查全部通过。
- README 两段和官网两段 XL 代码实际运行，输出与文档一致。

## 页面检查

Playwright 使用 1280x900 与 360x800 视口检查两张详情页，已查看截图：

- 页面无横向溢出，代码块在自身区域可滚动，无正文/表格重叠。
- 本地资源、内部锚点和导航存在，没有脚本异常。
- 官网目录搜索 chardet / graph 后点击能进入对应详情页。
- 包内正文与生成的官网正文完全相同；目录的总数、C 数和科学分类数与实际节点一致。

## 仍未闭环

1. **含 NUL 的语言字符串。** 原有两个失败用例仍失败：string.byteLen 截短、graph 名称混同。
   Cgen 显式常量最初使用 `xrtStrDupN(..., 3)` 创建 `a\0b`，但参数准备已经使用
   `xrtStrDup` 复制，之后字节长度桥又使用 `xrtStrView(const char*)`。
   因此不是 graph 名称哈希错误，也不只是 byteLen 里的一次 strlen；普通复制、参数、返回、
   容器装箱及相应桥接都需要统一保留长度。不能修改 XRT 的 C 字符串函数含义，也不能在包内
   增加名字映射、全局长度表或禁止原设计输入来规避。
2. **string.join 缺符号。** 标准库 `xl6StdStringJoinBody` 和外部声明存在，但
   `xl6IrStringBridgeContracts`、runtime bridge kind、needs 与 Cgen 发射链没有 join。
   后续要按现有严格合同接入 XRT join 和统一值转文本能力，联动错误、所有权、模块裁剪、
   生成原型及实际 JIT/AOT/DLL 验证；不能只手工注入一个同名符号让一个用例通过。
3. **语言迭代。** CSV class 字段数组迭代的共享 XLIR 问题仍由原失败用例追踪。
4. 未验证 Linux xlang JIT/AOT、32 位/ARM、TSan、全部算法性能或所有平台输入空间。

这些问题属于长期目标的待完成事项，图算法门禁的 `releaseReady` 仍为 false。
chardet 的 Buffer/C 显式字节接口完整，但不据此宣称语言 string 的 NUL 路径已经通过。

## 可复核材料

- [机器证据与输入哈希](extlib_chardet_graph_2026-09-28.delivery.json)
- [chardet Windows 原始检查](extlib_chardet_2026-09-28.delivery.windows.results.json)
- [graph Windows 原始检查](extlib_graph_2026-09-28.delivery.windows.results.json)
- [chardet README](../release/extlibs/chardet/README.md)
- [graph README](../release/extlibs/graph/README.md)
- chardet Linux 输出（历史记录：`../build/test/chardet-delivery-20260928-final/linux/gate.log`）
- graph Linux 输出（历史记录：`../build/test/graph-delivery-20260928-final/linux/gate.log`）
- [chardet 官网源码](../release/extlibs/chardet/docs/site.html)
- [graph 官网源码](../release/extlibs/graph/docs/site.html)
