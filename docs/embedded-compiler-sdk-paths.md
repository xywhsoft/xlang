# Embedded compiler SDK search paths

The Only-TCC provider keeps public declarations at `include/` and its compiler
source declarations at `__xl_compiler/include/` in the embedded catalog. These
paths are part of the matched SDK layout, including source-free installations.

The generated module is compiled before compiler implementation objects are
appended. Consequently, `xlConfigureTccStateEnvironment` adds the real public
`include/` root during initial state setup whenever the existing optional VFS
transport is available. Adding source paths only while appending implementation
objects is too late for a generated wrapper which already includes `xl.h`.

Compiler source compilation also adds `__xl_compiler/include/`. A declaration
SDK stored on disk uses the corresponding `include/__xl_compiler/include/`
directory. Existing module-relative SDK discovery remains in place. These are
current catalog directories; they do not introduce alternative historical SDK
layouts.

This adds one optional capability check and include registration per TCC state,
and one source include directory when compiling embedded implementation sources.
It adds no ordinary generated method calls or resource bookkeeping. It uses the
existing eight optional VFS slots; a plain TCC provider with no VFS exports keeps
its existing behavior.

`tools/check_source_provider_nested_linux.py` verifies the selected full provider
and accepts an explicitly partial compiler `--overlay` for a focused regression.
The report distinguishes the compiler overlay from the separately qualified
serialized provider objects. It copies declarations only beside the executable,
clears source/include environment overrides, runs from an empty work directory,
and tests seven programs through JIT, module, standalone compilation, dependency
inspection and relocated execution. One program uses `NativeSequence` directly;
another compiles it through an inner `xlang.Context`. A double-nested compiler
also imports `xlang` in the inner program, checking the serialized compiler's
own generated-wrapper SDK setup. Linux system libc headers,
CRT and linker inputs remain required.

Example with a final provider:

```text
python3 -B tools/check_source_provider_nested_linux.py \
  --root /path/to/frozen-source \
  --public-directory /path/to/matched-provider \
  --out /path/to/fresh-report
```

The separate `check_tcc_optional_vfs.py` gate builds a genuine plain TCC provider
and checks all eight optional exports, signature rejection, and both VFS and
non-VFS execution at O0/O2.
