# Linux 兼容性设计

本文档固定 DEMO6 的 Linux 平台边界、构建产物、内嵌资源和验收口径。Linux 与 Windows 使用同一套语言、AST、语义、代码生成、XRT 和 TCC 集成实现，不建立 Linux 专用语言分支。

## 第一阶段目标平台

- CPU：x86_64。
- ABI：System V AMD64 ABI。
- 用户态：glibc Linux。
- 开发基线：WSL2 Ubuntu 24.04 与 GitHub Actions `ubuntu-latest`。
- 编译工具：GCC、GNU Make、Python 3。

第一阶段不承诺 musl、ARM64、macOS 或其他 Unix 平台。后续支持这些平台时，应增加独立的目标清单和回归矩阵，不在当前路径中加入猜测式兼容回退。

## 构建产物

- `release/xl`：Linux CLI，动态链接同目录的 `libxlc.so`。
- `release/libxl.so`：运行版嵌入库，可在内存中运行 xlang 和 C，不输出 ELF 文件。
- `release/libxlc.so`：编译版嵌入库，可运行脚本并输出 ELF 可执行文件和共享库。

Linux 产物与 Windows 的 `xl.exe`、`xl.dll`、`xlc.dll` 可以同时存在于 `release`。平台产物不互相覆盖。

## 构建入口

Ubuntu/WSL2 安装依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential python3
```

构建与验收：

```bash
cd /mnt/d/GIT/x-lang/demo6
bash build.sh
bash test_linux.sh
```

`build.sh` 在 Linux 调用 `build_linux.sh`，在 Windows 调用 `build.bat`。Linux 构建不要求预装 TCC，不读取仓库外的 XRT 或 TCC 文件。

## TCC 与 VFS

Linux 构建从 `lib/tcc` 生成本机 TCC 和 `libtcc1.a`，随后将所需资源按清单压缩进入 `libxl.so` 或 `libxlc.so`。

资源查找顺序保持统一：

1. 当前编译上下文的动态 VFS。
2. 外部文件系统。
3. 只读静态 VFS。

动态 VFS 保存本次编译生成的 C 源和运行时源，随上下文释放。普通 `xl run` 不生成临时 C 文件或临时 ELF 文件。

静态 VFS 只保存 XLang、XRT 和 TCC 自有资源。Linux 的 glibc 头文件、C 启动对象、系统库和动态加载器来自目标系统，不复制进 XLang 发布物。这些文件属于目标 Linux ABI，内嵌副本会妨碍系统安全更新并降低平台兼容性。

## XRT 与系统库

生成的 Linux 程序默认使用 XRT，并由编译器自动链接：

- `pthread`
- `dl`
- `m`

用户无需为普通 xlang 程序手工添加这些链接参数。C 源通过 `xl run file.c` 运行时，宿主 XRT 符号自动注册到 TCC 上下文；通过 `xl build file.c` 输出 ELF 时，按同一规则生成独立产物。

## 路径与扩展库

- 源文件、命令行路径和公开 API 路径统一使用 UTF-8。
- 内部虚拟路径统一使用 `/`。
- `XLANG_PACKAGE_PATH` 在 Linux 使用 `:` 分隔，在 Windows 使用 `;` 分隔。
- package 的 Linux 动态依赖使用 `.so` 文件；清单应显式声明真实文件名，不做 `.dll` 到 `.so` 的兼容映射。
- Windows 资源文件和 `impdef` 只在 Windows 提供；Linux 遇到相关请求时返回明确的不支持错误。

## 发布验收

`test_linux.sh` 至少覆盖：

- CLI 启动、词法语法检查和 xlang 内存运行。
- `xl run` 前后临时目录不发生变化。
- xlang 到 ELF 的构建与运行。
- C 源的内存运行、XRT 环境注入和 ELF 构建。
- 中文目录及中文文件名。
- source import 与 package import。
- `libxl.so` 运行版宿主 API。
- `libxlc.so` 编译版宿主 API。

GitHub Actions 必须执行同一套本机 Linux 构建与验收。Windows 全回归仍是独立守门项，Linux 适配不得削弱 Windows 行为。
