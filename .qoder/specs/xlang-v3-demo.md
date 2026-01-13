# xlang 第三版 Demo 开发计划

## 概述

在 `demo/` 目录从头实现 xlang 编译器 Demo，完成阶段 0-5（词法→语法→语义→代码生成→TCC运行），实现完整的 Token 枚举，使用 xvalue 表达 AST。

## 目录结构

```
demo/
├── main.c                      # 扩展入口（已存在）
├── build.bat / build.sh        # 更新编译脚本
├── src/                        # 新建：编译器源码
│   ├── xl_common.h             # 通用定义（Token枚举、结构）
│   ├── xl_error.h              # 错误处理模块
│   ├── xl_lexer.h              # 词法分析接口
│   ├── xl_lexer.l              # flex 词法规则
│   ├── xl_ast.h                # AST 构建辅助（xvalue）
│   ├── xl_parser.h             # 语法分析接口
│   ├── xl_parser.y             # lemon 语法规则
│   ├── xl_semantic.h           # 语义分析模块
│   ├── xl_codegen.h            # C 代码生成模块
│   └── xl_backend.h            # TCC 后端模块
├── gen/                        # 新建：生成的文件
│   ├── xl_lex.c                # flex 生成
│   └── xl_parse.c/h            # lemon 生成
└── release/examples/           # 测试脚本
```

## 实施步骤

### 阶段 0：基础设施
1. 创建 `demo/src/` 和 `demo/gen/` 目录
2. 实现 `xl_common.h`：
   - 完整的 `XL_TokenKind` 枚举（约100个Token）
   - `XL_Token` 结构体
   - 工具宏和类型定义
3. 实现 `xl_error.h`：
   - `XL_Error` 和 `XL_ErrorList` 结构
   - 错误码定义（1000-6999）
   - 错误报告函数

### 阶段 1：词法分析器
4. 编写 `xl_lexer.l`（flex规则）：
   - 关键字识别（所有语言关键字）
   - 字面量：整数（十进制/十六进制/八进制/二进制）、浮点、字符串（4种）、时间
   - 运算符：单字符和多字符（按长度优先匹配）
   - 注释和空白处理
   - 中文标识符支持
5. 实现 `xl_lexer.h`：
   - `XL_Lexer` 上下文结构
   - `xlLexerCreate/Destroy/Next` 接口
   - `xlLexerDumpTokens` 调试函数
6. 生成并测试：`tools/flex.exe xl_lexer.l`

### 阶段 2：语法分析器
7. 实现 `xl_ast.h`：
   - AST 节点创建函数（使用 xvalue table）
   - `xlAstPrint` 递归打印函数
   - 节点字段：kind/line/col + 各节点特定字段
8. 编写 `xl_parser.y`（lemon规则）：
   - 按 spec.md 的 EBNF 实现语法
   - 优先级和结合性声明
   - 语义动作调用 `xlAstCreate*` 函数
9. 实现 `xl_parser.h`：
   - `XL_Parser` 上下文结构
   - `xlParserCreate/Parse/Destroy` 接口
10. 生成并测试：`tools/lemon.exe xl_parser.y`

### 阶段 3：语义分析器
11. 实现 `xl_semantic.h`：
    - `XL_Scope` 作用域结构（嵌套链表）
    - `XL_Symbol` 符号结构
    - 三遍扫描：符号收集→类型检查→闭包分析
    - `xlSemanticAnalyze` 入口
    - `xlSemanticDumpSymbols` 调试输出

### 阶段 4：代码生成器
12. 实现 `xl_codegen.h`：
    - `XL_CodeGen` 上下文（输出缓冲区）
    - 命名规则：`xlFnMain_*`、`xlM_*_*`、`xlMeta_*_*`
    - AST 节点到 C 代码映射
    - `xlCodeGenGenerate` 入口
    - `xlCodeGenSaveToFile` 文件输出

### 阶段 5：后端运行
13. 实现 `xl_backend.h`：
    - `XL_Backend` TCC 封装
    - `xlBackendCompile` 编译 C 代码
    - `xlBackendRun` 执行入口函数
14. 集成 `main.c`：
    - 命令行参数解析
    - 模式分发：`--dump-tokens/--dump-ast/--dump-sem/--emit-c/--run`
15. 更新 `build.bat/sh`：
    - 添加 flex/lemon 生成步骤
    - 编译所有源文件

## 关键文件

| 文件 | 职责 |
|------|------|
| `demo/src/xl_common.h` | Token 枚举（100个）、Token 结构、通用定义 |
| `demo/src/xl_lexer.l` | flex 词法规则，所有 Token 识别 |
| `demo/src/xl_parser.y` | lemon 语法规则，完整 EBNF 实现 |
| `demo/src/xl_semantic.h` | 符号表、类型检查、闭包分析 |
| `demo/src/xl_codegen.h` | AST → C 代码转换 |
| `demo/src/xl_backend.h` | TCC 编译与执行 |
| `demo/main.c` | 命令行入口，模式分发 |

## 测试验证

### 命令行测试
```bash
# 词法分析测试
xlang --dump-tokens release/examples/test.xl

# 语法分析测试
xlang --dump-ast release/examples/test.xl

# 语义分析测试
xlang --dump-sem release/examples/test.xl

# 代码生成测试
xlang --emit-c -o release/cgen/test.c release/examples/test.xl

# 运行测试
xlang --run release/examples/test.xl
xlang release/examples/test.xl  # 默认 --run
```

### 测试用例
- `release/examples/basics/` - 基础语法测试
- `release/examples/functions/` - 函数定义与调用
- `release/examples/classes/` - 类与对象
- `release/examples/closures/` - 闭包测试
- `release/examples/full/` - 综合功能测试

### 验证标准
1. `--dump-tokens`：Token 类型、位置、值正确
2. `--dump-ast`：AST 结构完整、层次清晰
3. `--dump-sem`：符号解析、类型标注正确
4. `--emit-c`：生成的 C 代码可用 GCC 编译
5. `--run`：执行结果符合预期

## 代码规范

- 缩进：tab
- 命名：匈牙利命名法（i/f/s/b/p/arr/tbl 前缀）
- 函数花括号：独立一行
- if/for/while 花括号：同行，括号内外加空格
- 空行：功能块间 1 行，模块间 3 行
