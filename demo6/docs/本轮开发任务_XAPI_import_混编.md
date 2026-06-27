# DEMO6 本轮开发任务：XAPI、import 与 C 混编成熟化

本文档记录本轮长期开发任务的唯一执行清单。目标是让 DEMO6 已实现的基础能力进入可组合、可分发、可混编、可长期维护的阶段。

## 1. 总目标

本轮不继续堆叠高级语法，优先完成以下能力：

- XAPI 接口元数据：作为 xl 的预解析/接口声明产物。
- 零配置 C 混编：支持 `xl build main.xl helper.c -o app.exe`。
- 自动链接 XRT：XRT 是 xlang 基础，不要求用户手写链接参数。
- C 自动注入头文件：C 文件不需要手写 `#include "xrt.h"` 或生成头文件。
- main 入口约定：C 或 xlang 均可实现 main，但一个目标只能有一个 main。
- `xlInit` / `xlUnit` 生命周期：命名与 XRT 的 `xrtInit` / `xrtUnit` 保持一致。
- shared library 生命周期：平台入口只做轻量 attach/detach，不直接运行复杂 xlang 用户代码。
- 最小 package/import：普通 `.xl` 是接口真相，package JSON 只负责定位能力和构建信息。
- 测试与范例：所有新增能力必须有可运行回归。

## 2. XAPI 接口元数据

XAPI 暂以 `.xapi.json` 形式落地，后续可增加二进制 `.xapi`。

XAPI 不是用户常规手写源，而是机器生成的接口元数据：

- 开发期：`.xl` 是权威源，`.xapi.json` 是缓存/调试产物。
- 分发期：`.xapi` 可以作为权威接口，供二进制扩展库 import。
- C 混编：`.xapi` 是生成 C header 的源。
- package/import：`.xapi` 负责导入命名空间、链接信息和 ABI 信息。
- 文档：未来 `#doc ... #end` 进入 `.xapi`，再生成帮助文档。

第一版 XAPI 字段：

```json
{
  "format": "xlang-xapi-1",
  "compiler": {
    "name": "xl",
    "version": "demo6",
    "xlangAbi": 1,
    "xrtAbi": 1
  },
  "target": {
    "os": "windows",
    "arch": "x64",
    "callingConvention": "cdecl"
  },
  "module": {
    "name": "main",
    "source": "main.xl",
    "sourceHash": ""
  },
  "entry": {},
  "imports": [],
  "exports": [],
  "types": [],
  "links": [],
  "loadlibs": [],
  "cSources": [],
  "docs": []
}
```

第一版重点记录：

- `export "c_symbol" func`
- `export "c_symbol" var`
- `export "c_symbol" const`
- `class`
- `custom`
- function signature
- C ABI symbol
- 多返回值 out 参数信息
- entry/lifecycle 信息

## 3. main 入口约定

本文立项时 DEMO6 尚无正式 xlang main 函数约定，只有顶层语句生成 `int main(void)` 的脚本模式。本轮已按以下规则收口；当前实现细节以 `C混编设计.md`、`命令行设计.md`、`ABI设计.md` 和 cgenrule 入口规则为准。

### 3.1 xlang main

正式入口：

```xlang
func main(...) as int
    return 0
end
```

规则：

- `main` 只能有一个。
- `main` 不能是 `extern`、`export`、泛型或重载函数。
- `main` 参数只允许 `...`，不允许固定参数或 `***`。
- `arg` 类型为 `array<var>`。
- `arg.length == argc`，`arg[0] == argv[0]`。
- 返回值允许省略或为 `int`；省略时退出码为 `0`。
- 多返回值 main 不允许。

生成 C：

```c
int main(int argc, char** argv)
{
    XL_InitOptions options;
    int code;

    options.size = sizeof(options);
    options.version = 1;
    options.argc = argc;
    options.argv = argv;
    options.reserved = 0;

    if ( !xlInit(&options) ) {
        return 1;
    }

    code = xl_user_main_from_argv(argc, argv);
    xlUnit();
    return code;
}
```

### 3.2 顶层语句

顶层执行语句保留脚本体验，视为脚本 main。

规则：

- 显式 `func main(...)` 与顶层执行语句不能同时存在。
- package/import 模块第一版不允许顶层执行语句。
- shared library 目标不允许顶层执行语句。

### 3.3 C main

如果构建 executable 时没有 xlang main，但存在 C 输入文件，则假定 C 代码提供 main。xl 不解析 C，也不检查 C main 是否存在，交给链接阶段验证。

C main 需要显式调用：

```c
int xlInit(const XL_InitOptions* options);
void xlUnit(void);
```

示例：

```c
int main(int argc, char** argv)
{
    XL_InitOptions options = xlInitOptions(argc, argv);
    int code;

    if ( !xlInit(&options) ) {
        return 1;
    }

    code = do_work();
    xlUnit();
    return code;
}
```

## 4. shared library 生命周期

`-shared` 目标不需要 main。

规则：

- `-shared` 不允许 xlang `main`。
- `-shared` 不允许顶层执行语句。
- `-shared` 自动生成生命周期 API。
- 平台入口只做轻量 attach/detach，不运行复杂 xlang 用户代码。

统一生命周期命名：

```c
int xlInit(const XL_InitOptions* options);
void xlUnit(void);
```

shared/package 真实导出符号需要唯一命名：

```c
xl_pkg_counter_init
xl_pkg_counter_unit
xl_pkg_counter_descriptor
```

Windows 平台入口：

```c
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    switch ( reason ) {
    case DLL_PROCESS_ATTACH:
        xlDllAttach(hinst);
        DisableThreadLibraryCalls(hinst);
        break;
    case DLL_PROCESS_DETACH:
        xlDllDetach(reserved != NULL);
        break;
    }
    return TRUE;
}
```

POSIX/macOS 口径：

```c
__attribute__((constructor))
static void xl_so_attach(void) { xlDllAttach(0); }

__attribute__((destructor))
static void xl_so_detach(void) { xlDllDetach(0); }
```

DEMO6 当前先落地 Windows/TCC 口径，同时在 XAPI 中记录跨平台生命周期字段。

## 5. CLI 与构建体验

目标命令：

```bat
xl build main.xl helper.c -o app.exe
```

要求：

- 支持多输入文件。
- `.xl` 输入生成 C 与 XAPI。
- `.c` 输入直接参与编译。
- C 输入自动强制 include `xrt.h` 与 xlang 生成 header。
- 自动链接 XRT、TCC 运行库和默认系统库。
- 用户仍可用 `--link`、`--lib-path`、`--inc-path`、`--loadlib` 添加额外依赖。
- `run` 命令仍以一个 `.xl` 为主，后续可扩展 C 输入。

默认链接由 `tools\xl6_toolchain_env.bat` 集中提供，不在各个命令、测试和文档里重复写死工具链路径。当前入口变量为：

```text
XL6_XRT_OBJ
XL6_TCC_LIBTCC1
Ws2_32
IPHLPAPI
Shell32
```

默认 include 同样由集中工具链环境提供：

```text
XL6_TCC_INCLUDES
XL6_TCC_INCLUDES_WINAPI
```

后续如果 XRT、TCC 或系统库路径变化，只调整 `tools\xl6_toolchain_env.bat` 和 driver/backend 的集中默认值，不把路径重新散落到 API、CLI、cgen、测试脚本或设计文档中。

## 6. package.json 与 import 第一版

不引入 `.xli`。不做 C parser。普通 `.xl` 模块是接口真相。

扩展库形态：

```text
counter/
    package.json
    src/counter.xl
    src/counter_native.c
```

第一版 package JSON：

```json
{
  "format": "xlang-package-1",
  "name": "counter",
  "version": "1.0.0",
  "entry": "src/counter.xl",
  "sources": {
    "c": ["src/counter_native.c"]
  },
  "links": [],
  "loadlibs": [],
  "dependencies": []
}
```

import 行为：

- `import counter` 查找本地 package。
- 读取 `package.json`。
- 解析 entry `.xl` 或读取分发期 `.xapi`。
- 只导入 `export` 符号。
- 符号进入 `counter.xxx` 命名空间，不污染全局。
- 自动加入 package 的 C 源码、链接库、动态库和 XAPI。

第一版只做本地包，不做远程包下载或完整包管理器。

## 7. 不做事项

本轮明确不做：

- C parser。
- `.xli`。
- 手写 XAPI 作为常规开发源。
- 完整包管理器。
- 远程包下载。
- 二进制 `.xapi`。
- 完整文档生成器。
- 异步、异常、object 元编程。

## 8. 开发顺序

1. 落盘本文档。
2. 梳理当前 CLI/CGEN/SEM/ABI 入口。
3. 实现 XAPI 生成器与 `xapi` 命令。
4. 让 `cgen --dump-dir` 输出 `.xapi.json`。
5. 让 header 逐步改为从 XAPI 信息投影。
6. 实现 `func main(...)` 约定、顶层脚本 main 互斥规则。
7. 实现 `xlInit` / `xlUnit` 与 `XL_InitOptions`。
8. 改造 build 支持多输入 `.xl + .c`。
9. 自动链接 XRT/default libs。
10. 自动强制 include 生成 header 与 xrt header。
11. 实现最小 package.json 解析。
12. 实现 `import package` 第一版。
13. 补测试与 feature_suite 范例。
14. 运行完整回归并修复问题。
