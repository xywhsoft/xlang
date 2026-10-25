# TCC VFS runtime header include mismatch (2026-10-25)

## Reproduce

The Windows release build (`demo6/build.bat`) completed successfully on
2026-10-25. From `demo6` run:

```text
release\xl.exe run dev\extlib_blockers\tcc_vfs_runtime_decl_include.xl
```

TCC fails before the package code runs:

```text
In file included from include/__xl_runtime/xl6_text_value.h:47:
include/__xl_runtime/xl6_tls_server_request.h:3: error: include file 'include/xruntime_decl.h' not found
```

The same error occurs with `release/extlibs/netrc/test/named_type.xl` and
`test/cases/extlib_pinyin.xl`. It also occurs in
`test/cases/extlib_rtree.xl` and
`test/cases/xlir_c_owned_import_exports.xl` (with
`XLANG_PACKAGE_PATH=test/fixtures;release/extlibs`). A plain script without
imports still runs. Other imports such as `release/examples/extlibs/slugify.xl`
and `release/examples/extlibs/zip.xl` also run successfully. This is a shared
blocker for imports that reach the affected runtime header, not for every
ordinary extension and not a `pinyin`, `netrc`, or `rtree` implementation error.

## Boundary and likely cause

The compiler VFS stamp contains `xruntime_decl.h` at
`include/__xl_runtime/xruntime_decl.h`, while
`src/runtime/xl6_tls_server_request.h` requests
`include/xruntime_decl.h`. The generated `xl6_text_value.h` includes the TLS
header, so a common package import reaches this path. The physical file
`release/include/xruntime_decl.h` exists, but that does not satisfy the
embedded-runtime lookup used here.

Fix the runtime header's include to use the canonical VFS location (or
change the VFS layout and every dependent include consistently). Check the
other newly embedded `xl6_*` headers for the same relative-include error;
then rebuild both DLLs and rerun this minimal import plus representative
package JIT/AOT gates. Do not add package-level include aliases or copies.

The first build attempt also failed the resource snapshot check because a
runtime header changed while packing. An immediate second build passed the
snapshot check and produced `xl.dll`, `xlc.dll`, and `xl.exe`; the include
failure above is from that completed build, not from the transient race.
