# 当前前端的语法与编辑投影适配

2026-10-30，M7a2b77a。接续 AST 基础适配，对齐提交 `9192a99f96ecd83bd57ca6e53df6dfcdda7d2f15` 的原生 Lexer／Lemon Parser、补全查询及参数提示投影。当前完整语言服务的语义、IR、模块输入与 XRT 进程适配仍有独立审核边界；开发基线保持 0.99，根 `release/` 未由本阶段修改。

## 复用原生前端

Parser API 与 scanner 派生器移入 `tools/native_signature_overlay.py`。旧资产与当前资产分别固定为完整摘要集合，覆盖 Parser API／头文件、Lemon C／原语法、scanner／字面量范围、AST 头文件、XRT 和补全 C／头文件。三个适配入口均在生成前核对集合，防止跨版本混搭。

当前 scanner、字面量规则和 Parser API 头文件仅有行尾编码差异，规范化文本相同。当前语法变化原样保留：

- 函数类型支持 `c_vararg` 参数；原生类型匹配器消费参数的可空后缀。
- 显式类型上下文中的嵌套 `>>` 继续由原匹配器拆分，不重写普通移位表达式。
- 原生 `@owned` 外部函数契约及对应 AST 标志。

沿用原 Parser 的 EOF／分隔符最终处理；该部分与旧资产相同。原 Lemon 联合类型、AST 栈值及排除控制符集合也相同；新增规则保留原有 AST 构造。派生只为丢弃的 AST 栈值补充析构，没有维护第二套语法或替换分析表。

## 编辑投影与错误处理

当前前端以 `xl6PathIdentityEqual` 提供三态路径比较。签名投影只接受明确相等，转换／资源错误会拒绝投影。原 `CompletionTarget` 现在会记录错误；只读签名缓存检查使用局部查询副本调用原接口，失败时拒绝复用，避免修改已发布查询。

同一标识符的光标前缀证明增加接收者编号、编译包标量及拥有型链接比较，包含 NULL／空串区别。模块、保留声明片段和类型边均须一致，否则走完整查询。旧布局经宏映射继续使用原比较代码。

## 独立构建与验证

新增 `tools/build_lsp_syntax.py`，复用 AST 目标的 Release 配置、依赖闭包、同一私有 AST 头文件检查和输入摘要。构建原生语法模块，并另编译未经改写的原 Parser API／Lemon C，以改名导出在同一测试中作独立对照。该目标不生成服务或 GUI。

当前资产全新编译 30 个对象，10 个 fixture 共 18,617 项检查通过：

| 检查 | 数量与范围 |
| --- | --- |
| AST 基础 | 2,504 项，复跑上一阶段 6 组，属于重复覆盖 |
| 光标增量语法 | 2,306 项，包含 40 代片段更新、独立来源、Unicode／BOM／CRLF、不完整成员访问与并发读者 |
| 语法存储 | 648 项，含原有真实分配失败恢复、预算和独立快照，以及新增字段的光标证明 |
| 调用位置索引 | 13,043 项，含原有 11 处 backing 分配故障逐点失败／恢复 |
| 当前 Parser 专项 | 116 项，比较未改写 Parser 的 AST／查询范围；覆盖新语法后的片段复用、不完整参数提示、泛型参数中的可空／变参函数类型、路径错误和只读查询保护 |

独立语法目标没有安装语义 raw-body 消费者。Windows PE 的 refptr 会保留其两个可选回调引用，因此测试链接明确的终止护栏：如果进入该未验收路径，立即打印错误并终止，不能返回模拟成功。完整服务继续链接原真实实现。本阶段不宣称覆盖 raw-body 语义接管。

两个负向对照分别移除新标量比较或链接比较，相同新 fixture 均以断言退出码 1 拒绝，没有用崩溃替代失败证据。六组未审核／混搭资产，经三个适配入口共 18 次尝试，全部在生成 C 文件前拒绝。

旧资产的三个派生 C 文件与已验收 0.99 逐字节一致。使用原编译参数，新编译的 `syntax.o`、`xl6_parser_api.o`、`xl6_lexer_gen.o`、`xl6_parser.o` 也逐字节一致。这只证明旧路径机器码保持，不据此宣称当前完整服务或 IDE 的速度已达标。

相同独立目标另以旧资产全新编译 28 个对象，8 个 fixture 的 18,392 项检查通过；与当前资产存在重叠覆盖，不将两者相加声称新增测试。两个构建的全部输入及输出摘要已再次核对，2,447 个固定前端文件保持不变。完整服务的 index／semantic／IR／completion／stdlib／types 守卫函数保持原样，当前完整预检仍拒绝未审核 index。

初次独立构建暴露只读 API 变化；后续发现测试链接的回调／wrap 配置缺项、可选基准中的旧时钟 API，以及新 fixture 的 typedef／匿名函数示例写法错误。失败记录和修正过程全部保留。可选基准改用两套 XRT 都提供的秒计时器转换为微秒；本阶段没有进行正式性能验收。

## 证据与后续

以仓库根目录为当前目录，使用已提交源码快照及一个不存在的新输出目录：

```powershell
python -X utf8 -B .\ide\tools\build_lsp_syntax.py --frontend-root .\ide\build\lsp-m76-20261030-5c81af92\frontend --output .\ide\build\syntax-substrate-new --cc E:\software\w64devkit\bin\gcc.exe
```

本轮实测 Windows x64／GCC 16.2，其他平台尚未验收。证据保存在 `ide/build/lsp-m77-20261030-b9a6da37/`：`current-final-v1/build-result.json`、`legacy-final-v1/build-result.json`、`legacy-generation-v1/result.json`、`legacy-objects-v1/result.json`、`profile-guards-v1/result.json`、`cursor-negative-v1/result.json`；统一复核见 `review-v1.json`。

下一步继续当前语义、IR、来源／模块加载、报告与 XRT 契约对齐，再进行完整原生／协议／IDE／性能验收。包信任、原生编号重绑定、完整语义键和实际增量生产不能由本阶段的语法比较替代；长期目标保持 active。
