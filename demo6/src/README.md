# demo6/src 源码目录

本目录保存 DEMO6 编译器和 DLL 运行库实现。源码按编译阶段、工程边界和运行时职责拆分，避免把语言前端、语义检查、代码生成、包解析和宿主 API 挤在同一个文件中。

## 分片职责

- `lexer`：Flex 词法定义、token 表示和词法 API。
- `parser`：Lemon 语法定义、parser 入口和语法产物。
- `ast`：AST 节点、节点构造和 AST dump。
- `sem`：语义检查、符号表、类型注册和基础诊断。
- `type`：类型模型、类型描述和类型辅助操作。
- `ir`：中间形态 dump，目前主要服务调试和规则核对。
- `cgen`：C 代码生成器。具体生成规则必须同步维护 `../cgenrule/README.md` 下的规则表。
- `abi`：ABI dump、XAPI dump/load 和 callable 描述。
- `package`：package manifest、resolver 和 import 依赖解析。
- `compile`：编译单元、import 注入和统一编译流程状态。
- `backend`：构建后端、工具链路径和 TCC/GCC/VC 边界。
- `stdlib`：标准库默认注册与 wrapper 入口。能落到 XRT 的功能优先调用 XRT。
- `api`：`xl.dll` 对外 API 实现。
- `driver`：`xl.exe` CLI 分派。
- `diag`：诊断输出和错误报告。
- `common`：极少量工程公共设施；文件、时间、内存、字符串等能力应优先使用 XRT。

## 生成文件

- `lexer/xl6_lexer_gen.c` 与 `lexer/xl6_lexer_gen.h` 由 `build.bat` 调用 Flex 生成。
- `parser/xl6_parser.c`、`parser/xl6_parser.h` 与 `parser/xl6_parser.out` 由 `build.bat` 调用 Lemon 生成。

生成文件可以参与构建与审查，但修改词法或语法时应优先改 `.l` / `.y` 源文件，再重新生成。

## 维护规则

- 新增 `.c` 文件必须接入 `build.bat`、`build_dll.bat` 或通过明确的 `#include "*.c"` 分片纳入构建。
- 不能把旧版本工具链路径、XRT 构建产物路径或临时绝对路径散落在源码中；工具链默认值集中在 `src/backend/xl6_toolchain.h` 和 `tools/xl6_toolchain_env.bat`。
- 代码生成诊断应给出具体错误，不能重新引入泛化的 `code generation error` 占位文案。
- XRT 已提供的文件、时间、内存、字符串、容器、同步等能力应优先调用 XRT。
