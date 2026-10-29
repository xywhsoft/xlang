# TCC VFS runtime header include mismatch (old release, 2026-10-25)

## Current recheck

After rebuilding the Windows release with `build.bat`, `xlc.dll` SHA-256 is
`4293404D9EDECF6BEAFBBCC0FC382261AACFA4B2811B5FD044B8CA0155901E6D`.
From the `demo6` root, the `pinyin`, `trie`, `units`, and `roaring` default
Windows package gates now pass, including their JIT and independent EXE checks.
An isolated directory containing only `xl.exe`, `xl.dll`, `xlc.dll`, the
`pinyin` source package, and its test program also passed `xl run`, `xl build`,
and execution of the built EXE. No `release/include` or other repository
headers were copied there. The failure below belongs to the earlier release
binary and is not a current blocker for these tested paths. This does not
claim every embedded header path or package has been exhaustively checked.

## Reproduce

An earlier Windows release build (`demo6/build.bat`) completed successfully on
2026-10-25. From `demo6` it reproduced with:

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
`XLANG_PACKAGE_PATH=test/fixtures;release/extlibs`). On the same release,
`test/cases/extlib_cli.xl` and the `tools/check_extlib_color.ps1` language
stage fail at the same header; the color gate's strict C tests, threads, and
independent named-color oracle pass before that point. A plain script without
imports still runs. Other imports such as `release/examples/extlibs/slugify.xl`
and `release/examples/extlibs/zip.xl`, as well as the basic color example,
also run successfully. This is a shared
blocker for imports that reach the affected runtime header, not for every
ordinary extension and not a `pinyin`, `netrc`, or `rtree` implementation error.

The lookup also depends on the process working directory. With the same
`release/xl.exe` build, these two commands have different outcomes:

```text
cd demo6
release\xl.exe run test\cases\extlib_rtree.xl     # missing header
cd demo6\release
xl.exe run ..\test\cases\extlib_rtree.xl        # exit 0
```

`release\xl.exe run release\extlibs\ini\test\boundaries.xl` fails from
`demo6` with the same header error, while the INI package gate changes its
working directory to `release` and passes JIT and AOT. The package gate is
valid for that directory only; this does not close the shared compiler bug.

## Boundary and likely cause

The compiler VFS stamp contains `xruntime_decl.h` at
`include/__xl_runtime/xruntime_decl.h`, while
`src/runtime/xl6_tls_server_request.h` requests
`include/xruntime_decl.h`. The generated `xl6_text_value.h` includes the TLS
header, so a common package import reaches this path. The physical file
`release/include/xruntime_decl.h` exists. Running from `release` can find
that external fallback; running from its parent cannot. The embedded runtime
therefore is not self-contained for this include.

Fix the runtime header's include to use the canonical VFS location (or
change the VFS layout and every dependent include consistently). Check the
other newly embedded `xl6_*` headers for the same relative-include error;
then rebuild both DLLs and rerun this minimal import plus representative
package JIT/AOT gates from both the release directory and its parent. Do not
add package-level include aliases or copies.

The first build attempt also failed the resource snapshot check because a
runtime header changed while packing. An immediate second build passed the
snapshot check and produced `xl.dll`, `xlc.dll`, and `xl.exe`; the include
failure above is from that completed build, not from the transient race.
