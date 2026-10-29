# pkgdemo — import / package / 扩展库验证工具

压测 `DEMO6功能清单.md` 第 16 节 import / package / 扩展库的全部公开能力。`dev` 项目群中专门覆盖模块化与扩展库方向的最小可用验证集。

## 目标

| # | 能力 | 本工具覆盖位置 |
|---|---|---|
| 1 | **source import**：`import srclib` 导入同目录 `.xl` 模块 | `pkgdemo.xl` → `srclib.xl`（同目录） |
| 2 | **package import**：导入有 manifest 的 package | `pkgdemo.xl` → `mylib/xlang.package.json` |
| 3 | **同名 C companion**：`.xl` 同名 `.c` 自动参与编译 | `srclib.xl`+`srclib.c`、`mylib.xl`+`mylib.c` |
| 4 | **导出符号**：`export func`/`export var`/`export const` | `mylib.xl` 的 `libAdd`/`libVersion`/`LIB_MAGIC` |
| 5 | **XLANG_PACKAGE_PATH**：环境变量指定搜索路径 | 见下方「验证」第 3 步 |

## 文件

```
pkgdemo/
    pkgdemo.xl              主程序，import srclib + mylib 并逐项自检
    srclib.xl               source import 模块（与主程序同目录）
    srclib.c                srclib.xl 的同名 C companion（自动编译）
    mylib/                  package 目录
        xlang.package.json  package manifest（xlSources 指向 mylib.xl）
        mylib.xl            package 源码（export func/var/const）
        mylib.c             mylib.xl 的同名 C companion（自动编译）
```

## 关键设计

### source import vs package import

两种 import 走不同的解析路径，导出写法也不同：

| 维度 | source import（`srclib`） | package import（`mylib`） |
|---|---|---|
| 定位方式 | 主源文件所在目录下的 `<name>.xl` | `<name>/xlang.package.json` |
| 导出写法 | `export "c_sym" func name(...)`（C ABI bridge） | `export func name(...)`（package 命名空间） |
| 变量导出 | `export "c_sym" Type name = value` | `export Type name = value` / `export const Type name = value` |
| manifest | 不需要 | 需要 `xlang.package.json` |

> source import 之所以要求 `export "c_sym"`，是因为同目录 `.xl` 模块通过 C ABI 符号桥接（参见 fixture `import_source_math.xl`）。package import 的 `export func`（无 c_sym）由 import 桥自动改名为内部符号（如 `__xl6_import_mylib_libAdd`）并合并进主编译单元。

### 同名 C companion 自动编译

import 解析器在展开每个 `.xl` 模块时会检查同目录同名的 `.c` 文件（`xl6ImportsAddSourceCompanionC`，src/compile/xl6_imports.c:314）。本工具同时验证两条路径：
- source import：`srclib.xl` → 自动加入 `srclib.c`
- package xlSource：`mylib.xl` → 自动加入 `mylib.c`（即便 manifest 的 `cSources` 为空）

> 因此 `mylib/xlang.package.json` 的 `cSources` 故意留空——`mylib.c` 完全靠同名 companion 机制被发现，与 manifest 显式声明的 `cSources`（如 fixture `counter`）区分开。

### package 搜索顺序

`xl6PackageResolveImport`（src/package/xl6_package.c:1041）按以下顺序查找 package：
1. 主源文件所在目录下的 `<target>/xlang.package.json`（相对解析）
2. `XLANG_PACKAGE_PATH` 环境变量列出的各目录（`;` 分隔）
3. `xl.exe` 同级 `extlibs/<target>/xlang.package.json`

本工具的 `mylib` 既可被第 1 步（相对解析，`pkgdemo/mylib/`）找到，也可被第 2 步（`XLANG_PACKAGE_PATH`）找到。

## 验证

```bat
cd /d D:\GIT\x-lang\demo6\release

:: 1. xl run（相对解析找到 mylib；EXIT=0）
xl.exe run projects\dev\pkgdemo\pkgdemo.xl

:: 2. xl build（两条路径都必须 EXIT=0）
xl.exe build projects\dev\pkgdemo\pkgdemo.xl -o projects\dev\pkgdemo\pkgdemo.exe
projects\dev\pkgdemo\pkgdemo.exe

:: 3. XLANG_PACKAGE_PATH 单独验证（把 envprobe 放到无 mylib 子目录的临时目录）
set XLANG_PACKAGE_PATH=projects\dev\pkgdemo
xl.exe run <tmp>\envprobe.xl
```

### 期望输出（run / exe 一致）

```
=== pkgdemo: import/package/extlib verification ===
[1] source import (srclib.xl + srclib.c)
  [ok] srclib.inc(41)
  [ok] srclib.inc(0)
  [ok] srclib.base()
[2] package import (mylib/xlang.package.json)
  [ok] mylib.libAdd(3,4)
  [ok] mylib.libAdd(100,23)
  [ok] mylib.libVersion
  [ok] mylib.LIB_MAGIC
[3] package same-name C companion (mylib.c)
  [ok] mylib.libMagic(1)
  [ok] mylib.libMagic(2)
=== pkgdemo done: 0 failure(s) ===
ALL CHECKS PASSED
```

## 实测结论

- 5 项能力全部通过，`xl run` 与 `xl build` 均 EXIT=0。
- IR dump 确认两条 C companion 路径（`xl6_pkgdemo_srclib_native_inc`、`xl6_pkgdemo_mylib_native_magic`）都被正确 extern 并调用。
- package 的 `export func`/`export string`/`export const int` 均被 import 桥改名为 `__xl6_import_mylib_*` 内部符号并被主程序访问。
- 未发现 BUG。
