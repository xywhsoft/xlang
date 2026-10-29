# DEMO6 extlib native artifact TODO

当前主线 import/package 第一版只把 `xlang.package.json` 中的 `xlSources` 和 `cSources` 作为直接构建输入。

多个 C 源文件组成的扩展库暂时仍以 `cSources` 清单表达。后续需要设计更完整的 native artifact 管理，用于处理：

- 多 C 源编译缓存。
- `.o/.obj/.a/.lib` 跨编译器兼容。
- 第三方源码目录的 include path。
- 调试版/发布版 native artifact。
- package lock 与 artifact hash。

## 当前多 C 源扩展库

| package | C 源数量 | 当前 `cSources` |
| --- | ---: | --- |
| `lz4` | 2 | `lz4_native.c`, `third_party/lz4.c` |
| `lzma` | 6 | `lzma_native.c`, `third_party/Alloc.c`, `third_party/CpuArch.c`, `third_party/LzFind.c`, `third_party/LzmaDec.c`, `third_party/LzmaEnc.c` |
| `md4c` | 4 | `md4c_native.c`, `third_party/md4c/md4c.c`, `third_party/md4c/md4c-html.c`, `third_party/md4c/entity.c` |
| `miniz` | 5 | `miniz_native.c`, `third_party/miniz.c`, `third_party/miniz_tdef.c`, `third_party/miniz_tinfl.c`, `third_party/miniz_zip.c` |
| `sqlite` | 2 | `sqlite_native.c`, `third_party/sqlite3.c` |
| `zstd` | 2 | `zstd_native.c`, `third_party/zstd.c` |

## 当前约束

- 第一版不自动纳入 `.o` 或 `.obj`。
- 第一版不缓存 C 编译产物。
- 需要 C 函数暴露给 xlang 时，仍必须在对应 `.xl` 中显式 `extern` 声明。
- 该文件是后续 native artifact 设计清单，不作为旧接口 JSON 机制的恢复入口。
