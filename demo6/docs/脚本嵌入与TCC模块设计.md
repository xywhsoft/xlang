# 脚本嵌入与 TCC 模块设计

本文档记录 DEMO6 后续实现“程序内运行 C / xlang 脚本、按需嵌入脚本能力、TCC 标准库模块、XRT 自动裁剪”的当前设计口径。

本文档是当前设计来源。若历史阶段文档、旧任务清单或旧 demo 口径与本文冲突，以本文为准。

## 设计目标

xlang 需要同时满足三类场景：

1. 普通 xlang 程序：
   - 编译为 native 程序。
   - 只携带实际使用到的 XRT 模块和最小 xlang 生成代码运行时。
   - hello world 不应因为 xlang 具备脚本/编译能力而自动变大。

2. xlang 程序使用 C/TCC 脚本能力：
   - 通过标准库 `tcc` 模块运行 C 字符串、C 文件，或在编译器模式下输出 exe/dll/obj。
   - TCC 公开 API 应尽量按原版 libtcc 使用手感暴露，同时提供更符合 xlang 的对象化封装。
   - TCC 上下文默认注入 XRT 和当前 xlang 上下文导出的 C ABI 符号。
   - 内存运行 C 脚本时不重复编译一份 XRT，脚本通过符号表使用宿主已经链接的 XRT。

3. xlang 程序使用 xlang 脚本或编译能力：
   - `xl.dll` 可作为动态运行库，在内存中运行 xlang/C 脚本，但不输出编译产物。
   - `xlc.dll` 是完整编译器库，可运行、编译输出、并携带完整编译所需资源。
   - 由 xlang 编译出的普通 exe 若希望不依赖 `xl.dll` / `xlc.dll` 也能运行 xlang 代码，必须按需嵌入对应运行子集。

## 产物职责

### 普通生成 exe

默认只包含：

- 当前程序使用到的 xlang 生成 C 代码。
- 当前程序使用到的 XRT 模块。
- 必要的内部 C 运行时代码补丁。

默认不包含：

- TCC。
- xlang 编译器。
- xlang 动态脚本运行引擎。
- TCC 编译输出资源。

只有源码中显式 `import tcc`、`import xlang` 或 `import xlang.compiler` 时，才自动嵌入对应能力。

### xl.dll

`xl.dll` 是轻量动态运行库。

它支持：

- 创建 `XL_Context`。
- 运行 xlang 字符串脚本和脚本文件。
- 运行 C 字符串脚本和 C 文件。
- 在内存中执行 xlang parse / cgen / TCC compile / relocate / run。
- 导出 libtcc 公开 API。
- 导出 XRT 环境注册 API。
- 将 DLL 内部链接的 xlang、XRT、TCC 符号注入 TCC 内存执行环境。

它不支持：

- 输出 exe。
- 输出 dll。
- 输出 obj/lib。
- 携带完整 xlang compiler resource bundle。

因此，`xl.dll` 的“不能编译”指不能输出外部编译产物；它仍然可以为了运行脚本，在内存中完成 xlang 到 C、C 到 TCC module 的动态编译。

### xlc.dll

`xlc.dll` 是完整编译器库。

它支持：

- `xl.dll` 的全部运行能力。
- 编译 C/xlang 到 exe/dll/obj 等外部产物。
- 完整 TCC compiler VFS 资源。
- 完整 xlang compiler 资源。
- 后续自执行、自举和包编译能力。

### xl.exe

`xl.exe` 是命令行入口。

它不应持有独立的特殊编译逻辑，而应调用与 `xlc.dll` 一致的编译/运行组件。

## 标准库命名空间

本轮固定只新增两个命名空间：

```xlang
import tcc
import xlang
```

不单独设计 `c` 命名空间。C 语言脚本和 C 编译能力归入 `tcc` 模块：

```xlang
tcc.run(source)
tcc.runFile(path)
tcc.compile(source, output)
tcc.compileFile(path, output)
```

xlang 自身动态运行和编译能力归入 `xlang` / `xlang.compiler`：

```xlang
xlang.run(source)
xlang.runFile(path)

import xlang.compiler
xlang.compiler.compile(source, output)
xlang.compiler.compileFile(path, output)
```

## tcc 模块

### 自动嵌入规则

源码出现：

```xlang
import tcc
```

编译器自动启用：

- `feature.tcc.runtime`
- TCC runtime VFS 清单
- `tcc.Context` native-backed class
- `tcc.run` / `tcc.runFile`
- XRT 符号注入能力
- 当前 xlang 模块导出的 C ABI 符号注入能力

如果程序使用编译输出能力，例如 `tcc.compile`、`tcc.compileFile` 或 `Context.outputFile`，则还必须处于编译器模式：

- `xlc.dll`
- `xl.exe build`
- 或生成 exe 显式/自动嵌入了 compiler feature

`xl.dll` 环境中调用输出编译产物 API 必须报错：

```text
tcc output is not available in xl.dll runtime mode
```

### tcc.Context

`tcc.Context` 是 native-backed class，底层持有 `TCCState*`。

构造时默认执行：

1. `tcc_new`
2. 设置默认 C 选项，例如 `-std=c11 -D_CRT_SECURE_NO_WARNINGS`
3. 配置内置/外部 TCC include path、library path、libtcc1
4. 注入 Windows 默认系统库需求
5. 注入 XRT 符号
6. 注入当前 xlang 上下文导出的 C ABI 符号

默认注入 XRT 是 xlang 的正式口径。xlang 的 C/TCC 环境默认就是 C + XRT + xlang ABI 的开箱环境。

第一期不设计 `raw` 构造参数。如果未来需要完全原始的 TCC 环境，再考虑：

```xlang
tcc.Context(raw: true)
```

### 覆盖范围

第一期 `tcc.Context` 覆盖所有 `libtcc.h` 中的 `LIBTCCAPI` 公开 API，以及 xlang 扩展的 `tcc_builtin_vfs.h` 中的 `TCC_VFS_API`。

`xl.def` / `xlc.def` 中额外导出的 TCC 内部符号不进入 xlang 标准库承诺范围。例如 `tcc_malloc`、`tcc_parse_args`、`tcc_enter_state` 等若不在公开头文件中，不作为 `tcc` 模块 API。

### 常量

```xlang
tcc.MEMORY
tcc.EXE
tcc.DLL
tcc.OBJ
tcc.PREPROCESS
```

对应 libtcc：

```c
TCC_OUTPUT_MEMORY
TCC_OUTPUT_EXE
TCC_OUTPUT_DLL
TCC_OUTPUT_OBJ
TCC_OUTPUT_PREPROCESS
```

### 核心方法

```xlang
auto cc = tcc.Context()

cc.setLibPath(path)
cc.setOptions(options)
cc.addIncludePath(path)
cc.addSysIncludePath(path)
cc.defineSymbol(name, value = "")
cc.undefineSymbol(name)
cc.addFile(path)
cc.compileString(source)
cc.setOutputType(tcc.MEMORY)
cc.addLibraryPath(path)
cc.addLibrary(name)
cc.addSymbol(name, value)
cc.outputFile(path)
cc.run(args = [])
cc.relocate()
cc.getSymbol(name)
cc.listSymbols(callback)
cc.setErrorFunc(callback)
cc.setBacktraceFunc(callback)
```

说明：

- `outputFile` 只在编译器模式可用。
- `run` 对应 `tcc_run`，用于 TCC 的 C `main` 路径。
- `relocate` + `getSymbol` 是推荐的内存模块精细控制路径。
- `getSymbol` 返回 `point`，用户可赋值给明确签名的 `func(...) as ...`。

示例：

```xlang
import tcc

auto cc = tcc.Context()
cc.setOutputType(tcc.MEMORY)
cc.compileString("int add(int a, int b){ return a + b; }")
cc.relocate()

func(int, int) as int add = cc.getSymbol("add")
print(add(1, 2))
```

### XRT 注入

`tcc.Context()` 默认注入 XRT，不需要用户手写。

同时保留显式方法，便于用户在特殊情况下重新注入或确认语义：

```xlang
cc.registerXrt()
```

该方法对应 C API：

```c
int xlTccRegisterXrtEnvironment(TCCState* state);
int xlTccRegisterContextEnvironment(XL_Context* ctx, TCCState* state);
```

### 回调 API

回调类 API 第一阶段也要实现，但应包装成 xlang 函数手感，不暴露 C 风格 `void* opaque` 细节。

错误回调：

```xlang
cc.setErrorFunc(func(string message)
    print(message)
end)
```

符号遍历：

```xlang
cc.listSymbols(func(string name, point addr)
    print(name)
end)
```

backtrace 回调：

```xlang
cc.setBacktraceFunc(func(point pc, string file, int line, string funcName, string msg) as bool
    return true
end)
```

说明：

- 回调对象必须由 `tcc.Context` 持有引用，保证 TCC 使用期间不会被释放。
- `setErrorFunc` 的回调应接收 TCC 诊断文本；是否拆分文件、行号、严重级别可后续增强。
- `setBacktraceFunc` 返回 `true` 表示继续输出 backtrace，返回 `false` 表示停止。
- `_tcc_setjmp` / `tcc_setjmp` 属于异常捕获支撑能力。第一期可通过 `Context` 内部封装使用，不要求直接暴露为普通 xlang 函数。

### VFS 扩展 API

VFS API 进入 `tcc` 模块。

模块级：

```xlang
tcc.vfsMountMemory(path, data)
tcc.vfsClearDynamic()
```

实例级：

```xlang
cc.mountMemory(path, data)
```

对应公开 C API：

```c
tcc_vfs_mount_memory
tcc_vfs_clear_dynamic
```

`tcc_vfs_open/read/lseek/close/fopen/fclose` 是底层适配 API，不作为常规 xlang 调用 API 暴露给普通用户；除非后续有诊断需求，再进入专家接口。

### 快捷函数

```xlang
tcc.run(source, args = []) as int
tcc.runFile(path, args = []) as int
tcc.compile(source, output) as bool
tcc.compileFile(path, output) as bool
```

语义：

- `run` / `runFile` 创建临时 `tcc.Context`，默认注入 XRT，编译 C 源码，并运行 C `main`。
- `compile` / `compileFile` 创建临时 `tcc.Context`，默认注入 XRT，输出外部产物。
- `compile` / `compileFile` 只在编译器模式可用。

示例：

```xlang
import tcc

string code = """
int main(int argc, char** argv)
{
    return 0;
}
"""

int rc = tcc.run(code)
```

## xlang 模块

### 自动嵌入规则

源码出现：

```xlang
import xlang
```

编译器自动启用：

- `feature.xlang.runtime`
- xlang 动态运行子集
- xlang 源码到 C 的内存运行链路
- TCC runtime VFS
- XRT 符号注入能力

源码出现：

```xlang
import xlang.compiler
```

编译器自动启用：

- `feature.xlang.compiler`
- 完整 xlang 编译器资源
- xlang 编译输出 API
- TCC compiler VFS
- 编译外部 exe/dll 所需资源

### xlang runtime API

```xlang
xlang.run(source, arg = []) as int
xlang.runFile(path, arg = []) as int

auto ctx = xlang.Context()
ctx.run(source, arg = [])
ctx.runFile(path, arg = [])
ctx.addIncludePath(path)
ctx.addLibraryPath(path)
ctx.addLinkLibrary(nameOrPath)
ctx.registerCFunction(name, funcValue)
```

说明：

- `xlang.run` / `xlang.runFile` 只运行脚本，不输出编译产物。
- `xlang.Context` 对应 `XL_Context` 的 xlang native-backed class 包装。
- `arg` 必须是数组；未传入时使用空数组。

### xlang compiler API

编译输出能力放入 `xlang.compiler`，不放入普通 `xlang` 命名空间。

```xlang
import xlang.compiler

xlang.compiler.compile(source, output) as bool
xlang.compiler.compileFile(path, output) as bool
```

后续可扩展：

```xlang
xlang.compiler.check(source)
xlang.compiler.lex(source)
xlang.compiler.parse(source)
xlang.compiler.emitC(source)
xlang.compiler.emitAbi(source)
```

第一期优先实现运行闭环和外部产物输出闭环，不急于暴露调试 dump 类 API。

## 自动 feature 推断

编译器根据 import/package/source import 依赖图自动生成嵌入计划。

规则：

| 源码依赖 | 自动 feature |
| --- | --- |
| `import tcc` | `feature.tcc.runtime` |
| `import xlang` | `feature.xlang.runtime` |
| `import xlang.compiler` | `feature.xlang.compiler` |

命令行 `--with` / `--without` 可作为专家覆盖选项，但不作为日常使用方式。

动态字符串决定 import 的场景无法静态分析，用户必须显式 import 对应模块。例如：

```xlang
import xlang.compiler
```

否则运行时报错：

```text
xlang compiler is not embedded. Add `import xlang.compiler`.
```

## 资源分组

内置资源按 feature 分组，不允许重新回到“扫描目录全塞”的方式。

建议分组：

```text
tcc.runtime.vfs
tcc.compiler.vfs
xlang.runtime.sources
xlang.compiler.sources
xrt.headers
xrt.selected.sources
```

说明：

- `tcc.runtime.vfs`：运行 C/xlang 脚本所需的最小 TCC include、libtcc1、运行时资源。
- `tcc.compiler.vfs`：输出 exe/dll/obj 所需的额外 def、import lib、平台启动文件等资源。
- `xlang.runtime.sources`：动态运行 xlang 脚本所需的 lexer/parser/sem/cgen/runtime 子集。
- `xlang.compiler.sources`：完整编译器输出能力所需源码和元数据。
- `xrt.headers`：提供给编译输出路径使用的 XRT 单头文件或拆分头。
- `xrt.selected.sources`：按 XRT feature 裁剪后的源码集合。

## XRT 裁剪

第一期采用模块级裁剪，不做函数级裁剪。

编译器根据标准库使用和 feature 需求生成 XRT 裁剪配置：

```c
#define XRT_ENABLE_STRING 1
#define XRT_ENABLE_MATH 1
#define XRT_ENABLE_FILE 0
#define XRT_ENABLE_HTTP 0
```

普通程序只启用实际使用的 XRT 模块。

脚本能力有两种策略：

1. strict：
   - 只注入已声明 feature 需要的 XRT 模块。
   - 体积最小。
   - 动态脚本调用未嵌入模块时报错。

2. full-xrt：
   - 脚本环境注入完整 XRT。
   - 体积更大。
   - 更适合插件、工具、动态脚本平台。

默认策略第一期建议为 strict。命令行或构建配置可显式选择 full-xrt。

## 错误策略

缺失能力必须给出明确诊断，不做静默失败或兼容回退。

示例：

```text
tcc runtime is not embedded. Add `import tcc`.
tcc output is not available in xl.dll runtime mode.
xlang runtime is not embedded. Add `import xlang`.
xlang compiler is not embedded. Add `import xlang.compiler`.
```

## 实现顺序

第一阶段：

1. 拆出可复用的 TCC 环境配置层：
   - include/lib path
   - VFS 清单
   - libtcc1
   - XRT 符号注入
   - xlang C ABI 符号注入

2. 实现 `tcc` 标准库模块：
   - `tcc.Context`
   - libtcc 公开 API 包装
   - VFS 扩展 API 包装
   - 回调 API 包装
   - `tcc.run` / `tcc.runFile`

3. 实现编译器模式下的：
   - `tcc.compile`
   - `tcc.compileFile`
   - `Context.outputFile`

4. 实现 `xlang` 运行模块：
   - `xlang.run`
   - `xlang.runFile`
   - `xlang.Context`

5. 接入 import 驱动 feature 推断：
   - `import tcc`
   - `import xlang`
   - `import xlang.compiler`

第二阶段：

1. 将 `xl.dll` 运行子集工程化为可静态嵌入组件。
2. 生成 exe 根据 feature 自动嵌入 `tcc.runtime` / `xlang.runtime`。
3. 普通 hello world 验证不携带 TCC/xlang runtime。
4. `import tcc` 的生成 exe 可不依赖 `xl.dll/xlc.dll` 运行 C 脚本。
5. `import xlang` 的生成 exe 可不依赖 `xl.dll/xlc.dll` 运行 xlang 脚本。

第三阶段：

1. `xlang.compiler` 完整自执行能力。
2. XRT 模块级裁剪落地。
3. full-xrt / strict 脚本模式切换。
4. 编译缓存、资源压缩、模块热加载等优化。

## 验收标准

第一期验收：

- `import tcc` 后可创建 `tcc.Context()`。
- `tcc.Context()` 默认注入 XRT。
- `tcc.Context` 覆盖 `libtcc.h` 的公开 API。
- `tcc` 模块覆盖 `tcc_builtin_vfs.h` 的动态 VFS 扩展能力。
- 回调 API 可用，并能从 xlang 函数接收 TCC 诊断、符号遍历和 backtrace 回调。
- `tcc.run` 可运行 C 字符串。
- `tcc.runFile` 可运行 C 文件。
- `xl.dll` 中 `tcc.run` 可用，但 `tcc.compile` / `outputFile` 报编译模式不可用。
- `xlc.dll` / `xl.exe` 编译器模式中 `tcc.compile` / `outputFile` 可用。
- `import xlang` 后 `xlang.run` / `xlang.runFile` 可运行 xlang 脚本。
- 未 import 对应模块时调用相关能力必须给出清晰错误。

第二期验收：

- 普通 hello world 生成 exe 不携带 TCC 和 xlang runtime。
- `import tcc` 的生成 exe 不依赖 `xl.dll/xlc.dll`，可运行 C 脚本。
- `import xlang` 的生成 exe 不依赖 `xl.dll/xlc.dll`，可运行 xlang 脚本。
- `import xlang.compiler` 的生成 exe 可使用 xlang 编译输出能力。
- XRT 至少完成模块级裁剪，未使用模块不进入普通程序。

## 当前实现基线

截至本轮实现，以下能力已经进入 DEMO6 基线：

- `import tcc` 会自动启用 `feature.tcc.runtime`，并在外部 exe 产物中嵌入 libtcc 源码、运行版 TCC VFS 资源和 LZMA 解压代码。
- `TccContext` 创建时会默认配置动态 VFS 可见的 `include` / `include/winapi` / `lib` 路径，并注入 XRT 常用符号。
- `tcc.run`、`TccContext.compileString + run`、`tcc.vfsMount + tcc.runFile` 已在宿主运行态和生成 exe 产物中验证通过。
- `import xlang` 会自动启用 `feature.xlang.runtime`，并在外部 exe 产物中嵌入运行版 xlang API 源码集。
- `xlang.run`、`XLangContext.run` 已在宿主运行态和生成 exe 产物中验证通过；普通 `import xlang` 不携带外部 exe/dll 输出能力。
- `TccHandler` 与 `XLangHandler` 均提供 `point` 构造重载，用于常驻 handler 池显式接管已有句柄；临时借用者必须在离开作用域前将 `handle` 置空，避免重复释放。
- 外部 exe 输出路径已经接入 XRT 模块级裁剪基础，普通 hello world、crypto、regex、json、path/file 等样例均通过裁剪后产物验证。
- `release/examples/script_embed_tcc.xl` 和 `release/examples/script_embed_xlang.xl` 作为发布示例覆盖脚本嵌入主路径。
- `test/suites/09_artifact_closure.bat` 已加入独立 exe 的 `tcc` / `xlang` 脚本嵌入闭包测试。

尚未完成：

- `import xlang.compiler` 的完整自执行编译器能力仍按第三阶段处理。
- XRT 当前是模块级裁剪基础，不是函数级裁剪。

## 暂缓事项

- 函数级 XRT 裁剪。
- 完整沙箱和权限模型。
- 动态脚本 import 的自动静态推断。
- xlang 完整自举发行流程。
- 编译缓存和热更新。
